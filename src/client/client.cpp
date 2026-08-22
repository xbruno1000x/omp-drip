#include <Windows.h>

#include <MinHook.h>
#include <RakClientInterface.h>
#include <NetworkTypes.h>

#include "catalog.hpp"
#include "conflict_detector.hpp"
#include "file_hash.hpp"
#include "protocol.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace drip::client {

namespace {

using drip::protocol::Opcode;
using drip::protocol::Reader;
using drip::protocol::Writer;

constexpr std::uintptr_t kSetTextureAndModel = 0x5A8080;
constexpr std::uintptr_t kRebuildPlayer = 0x5A82C0;
constexpr std::uintptr_t kGameProcess = 0x53BEE0;
constexpr std::uintptr_t kSampNetGamePointerR3 = 0x26E8DC;
constexpr int kMaxPlayers = 1004;
constexpr int kConnectedState = 5;
constexpr std::size_t kRakClientReceiveVtableIndex = 8;
constexpr std::uint64_t kApplyDelayFrames = 2;

constexpr std::array<std::uint8_t, drip::protocol::kBuildGuidSize> kBuildGuid = {
    0x8D, 0xF0, 0x9E, 0x6A, 0x17, 0xBB, 0x4B, 0xDF,
    0x9E, 0xAE, 0xD2, 0x1B, 0x93, 0xAF, 0x13, 0x4C,
};

#pragma pack(push, 1)
struct SampPed {
    std::uint8_t pad[0x2A4];
    void* gtaPed;
};
struct RemotePlayer { SampPed* ped; };
struct PlayerInfo { RemotePlayer* player; };
struct LocalPlayer { SampPed* ped; };

struct PlayerPool {
    std::uint32_t maxPlayerId;
    PlayerInfo* remote[kMaxPlayers];
    std::int32_t listed[kMaxPlayers];
    std::uint32_t previousCollision[kMaxPlayers];
    std::int32_t localPing;
    std::int32_t localScore;
    std::uint16_t localId;
    std::int32_t alignment;
    std::uint8_t localName[24];
    LocalPlayer* local;
};

struct SampPools {
    void* menu;
    void* actor;
    PlayerPool* player;
    void* vehicle;
    void* pickup;
    void* object;
    void* gangZone;
    void* textLabel;
    void* textDraw;
};

struct SampNetGame {
    void* rakPeer;
    char unknown[40];
    RakNet::RakClientInterface* rakClient;
    char ip[257];
    char host[257];
    bool disableCollision;
    bool updateCameraTarget;
    bool nametagStatus;
    std::uint32_t port;
    std::int32_t lanMode;
    std::uint32_t mapIcons[100];
    std::int32_t gameState;
    std::uint32_t connectTick;
    void* settings;
    char unknown2[5];
    SampPools* pools;
};

struct PlayerDataPrefix {
    void* wanted;
    void* clothes;
};

struct ClothesDescription {
    std::uint32_t modelKeys[10];
    std::uint32_t textureKeys[18];
    float fat;
    float muscle;
};

struct SampPlayerIdR3 {
    std::uint32_t binaryAddress;
    std::uint16_t port;
};

struct SampPacketR3 {
    std::uint16_t playerIndex;
    SampPlayerIdR3 playerId;
    std::uint32_t length;
    std::uint32_t bitSize;
    std::uint8_t* data;
    bool deleteData;
};
#pragma pack(pop)

static_assert(sizeof(ClothesDescription) == 0x78);
static_assert(offsetof(SampPed, gtaPed) == 0x2A4);
static_assert(offsetof(PlayerPool, localId) == 12060);
static_assert(offsetof(PlayerPool, local) == 12090);
static_assert(offsetof(SampPools, player) == 0x08);
static_assert(offsetof(SampNetGame, rakClient) == 0x2C);
static_assert(offsetof(SampNetGame, pools) == 0x3DE);
static_assert(offsetof(SampPacketR3, data) == 0x10);
static_assert(sizeof(SampPacketR3) == 0x15);

using SetTextureFn = void(__fastcall*)(ClothesDescription*, int, const char*, const char*, int);
using RebuildFn = void(__cdecl*)(void*, bool);
using GameProcessFn = void(__cdecl*)();
using ReceiveFn = SampPacketR3*(__thiscall*)(void*);

struct AppliedRecord {
    std::uint32_t revision = 0;
    void* ped = nullptr;
    bool dirty = true;
};

HMODULE g_module = nullptr;
HMODULE g_sampModule = nullptr;
HANDLE g_ownership = nullptr;
Catalog g_catalog;
std::unordered_map<std::uint16_t, drip::protocol::AppearanceState> g_states;
std::unordered_map<std::uint16_t, AppliedRecord> g_applied;
std::unordered_map<std::uint16_t, std::uint64_t> g_applyAfterFrame;
std::optional<drip::protocol::AppearanceState> g_preview;
std::uint64_t g_gameFrame = 0;
std::array<Sha256, drip::protocol::kFileCount> g_fileHashes{};
Sha256 g_manifestHash{};
std::atomic<bool> g_running{true};
std::atomic<bool> g_accepted{false};
std::atomic<bool> g_helloSent{false};
drip::protocol::RejectReason g_clientStatus = drip::protocol::RejectReason::None;
std::string g_conflictModule;
thread_local bool g_applying = false;
std::mutex g_logMutex;

SetTextureFn g_originalSetTexture = nullptr;
RebuildFn g_originalRebuild = nullptr;
GameProcessFn g_originalGameProcess = nullptr;
ReceiveFn g_originalReceive = nullptr;

std::filesystem::path gameRoot() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return std::filesystem::path(path).parent_path();
}

void log(const std::string& message) {
    std::lock_guard lock(g_logMutex);
    auto directory = gameRoot() / L"omp-drip";
    std::error_code ignored;
    std::filesystem::create_directories(directory, ignored);
    std::ofstream stream(directory / L"client.log", std::ios::app);
    if (!stream) {
        wchar_t localAppData[MAX_PATH]{};
        if (GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData,
                static_cast<DWORD>(std::size(localAppData)))) {
            directory = std::filesystem::path(localAppData) / L"omp-drip";
            std::filesystem::create_directories(directory, ignored);
            stream.clear();
            stream.open(directory / L"client.log", std::ios::app);
        }
    }
    SYSTEMTIME time{};
    GetLocalTime(&time);
    stream << '[' << time.wHour << ':' << time.wMinute << ':' << time.wSecond << "] " << message << '\n';
}

bool readable(const void* pointer, std::size_t size = 1) {
    if (!pointer) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(pointer, &memory, sizeof(memory)) || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    const auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return begin <= end && size <= end - begin;
}

SampNetGame* netGame() {
    if (!g_sampModule) return nullptr;
    auto** pointer = reinterpret_cast<SampNetGame**>(
        reinterpret_cast<std::uintptr_t>(g_sampModule) + kSampNetGamePointerR3);
    return readable(pointer, sizeof(*pointer)) ? *pointer : nullptr;
}

PlayerPool* playerPool() {
    auto* game = netGame();
    return game && readable(game) && game->pools && readable(game->pools) ? game->pools->player : nullptr;
}

void* pedFor(std::uint16_t playerId) {
    auto* pool = playerPool();
    if (!pool || !readable(pool)) return nullptr;
    SampPed* sampPed = nullptr;
    if (playerId == pool->localId) {
        if (pool->local && readable(pool->local)) sampPed = pool->local->ped;
    } else if (playerId < kMaxPlayers && pool->listed[playerId] == 1) {
        auto* info = pool->remote[playerId];
        if (info && readable(info) && info->player && readable(info->player)) sampPed = info->player->ped;
    }
    return sampPed && readable(sampPed, sizeof(SampPed)) ? sampPed->gtaPed : nullptr;
}

ClothesDescription* clothesFor(void* ped) {
    if (!readable(ped, 0x484)) return nullptr;
    auto* playerData = *reinterpret_cast<PlayerDataPrefix**>(static_cast<std::uint8_t*>(ped) + 0x480);
    if (!readable(playerData, sizeof(PlayerDataPrefix))) return nullptr;
    return readable(playerData->clothes, sizeof(ClothesDescription))
        ? static_cast<ClothesDescription*>(playerData->clothes) : nullptr;
}

void markDirtyForPed(void* ped) {
    for (auto& [playerId, record] : g_applied) {
        if (record.ped == ped) record.dirty = true;
    }
}

void markDirtyForClothes(ClothesDescription* clothes) {
    for (auto& [playerId, record] : g_applied) {
        if (record.ped && clothesFor(record.ped) == clothes) record.dirty = true;
    }
}

bool applyState(const drip::protocol::AppearanceState& state, bool force) {
    void* ped = pedFor(state.playerId);
    auto* clothes = clothesFor(ped);
    if (!ped || !clothes || !g_originalSetTexture || !g_originalRebuild) return false;

    auto& applied = g_applied[state.playerId];
    if (!force && !applied.dirty && applied.ped == ped && applied.revision == state.revision) return true;

    g_applying = true;
    // Preserve torso, cabeca, pernas e calcados ate que seus novos valores
    // sejam aplicados. Zerar essas chaves estruturais antes de RebuildPlayer
    // deixa player.img modificados em um estado intermediario invalido.
    // Os slots opcionais podem ser limpos com seguranca e depois reaplicados.
    std::fill(std::begin(clothes->modelKeys) + 4, std::end(clothes->modelKeys), 0u);
    std::fill(std::begin(clothes->textureKeys) + 4, std::end(clothes->textureKeys), 0u);
    for (std::size_t slot = 0; slot < state.items.size(); ++slot) {
        const auto itemId = state.items[slot];
        if (itemId == 0) continue;
        const auto* item = g_catalog.find(itemId);
        if (!item || item->slot != slot) continue;
        g_originalSetTexture(clothes, 0, item->texture.c_str(), item->model.c_str(), item->slot);
    }
    clothes->muscle = static_cast<float>(std::min<std::uint16_t>(state.muscle, 1000));
    clothes->fat = static_cast<float>(std::min<std::uint16_t>(state.fat, 1000));
    g_originalRebuild(ped, false);
    g_applying = false;

    applied.ped = ped;
    applied.revision = state.revision;
    applied.dirty = false;
    return true;
}

void scheduleApply(std::uint16_t playerId) {
    g_applied[playerId].dirty = true;
    g_applyAfterFrame[playerId] = g_gameFrame + kApplyDelayFrames;
}

bool canApply(std::uint16_t playerId) {
    const auto it = g_applyAfterFrame.find(playerId);
    return it == g_applyAfterFrame.end() || g_gameFrame >= it->second;
}

void finishApply(std::uint16_t playerId, bool applied) {
    if (applied) g_applyAfterFrame.erase(playerId);
}

void applyPending() {
    if (!g_accepted) return;
    if (g_preview && canApply(g_preview->playerId)) {
        auto& applied = g_applied[g_preview->playerId];
        void* currentPed = pedFor(g_preview->playerId);
        finishApply(g_preview->playerId, applyState(*g_preview,
            applied.dirty || applied.ped != currentPed || applied.revision != g_preview->revision));
    }
    for (const auto& [playerId, state] : g_states) {
        if (g_preview && g_preview->playerId == playerId) continue;
        if (!canApply(playerId)) continue;
        auto& applied = g_applied[playerId];
        void* currentPed = pedFor(playerId);
        finishApply(playerId, applyState(state,
            applied.dirty || applied.ped != currentPed || applied.revision != state.revision));
    }
}

void __fastcall hookSetTexture(ClothesDescription* clothes, int unused,
    const char* texture, const char* model, int slot) {
    g_originalSetTexture(clothes, unused, texture, model, slot);
    if (!g_applying) markDirtyForClothes(clothes);
}

void __cdecl hookRebuild(void* ped, bool ignoreBody) {
    g_originalRebuild(ped, ignoreBody);
    if (!g_applying) markDirtyForPed(ped);
}

void processPacket(const SampPacketR3& packet) {
    if (!packet.data || packet.length < Writer::headerSize() || packet.length > drip::protocol::kMaxPacketBytes
        || !readable(packet.data, packet.length) || packet.data[0] != drip::protocol::kPacketId) return;
    Reader reader(packet.data, packet.length, true);
    Opcode opcode{};
    if (!reader.readHeader(opcode)) return;

    if (opcode == Opcode::ServerAccepted) {
        std::uint16_t version = 0;
        if (reader.read(version) && reader.empty() && version == drip::protocol::kVersion) {
            g_accepted = true;
            log("Handshake aceito pelo servidor.");
        }
        return;
    }
    if (opcode == Opcode::ServerRejected) {
        std::uint8_t reason = 0;
        std::uint8_t length = 0;
        std::array<char, 221> message{};
        if (reader.read(reason) && reader.read(length) && length <= 220
            && reader.readBytes(message.data(), length) && reader.empty()) {
            log(std::string("Handshake rejeitado: ") + message.data());
        }
        return;
    }
    if (opcode == Opcode::FullState || opcode == Opcode::StateUpdate || opcode == Opcode::Preview) {
        drip::protocol::AppearanceState state;
        if (!drip::protocol::readState(reader, state)) return;
        if (opcode == Opcode::Preview) {
            g_preview = state;
            scheduleApply(state.playerId);
        } else {
            auto it = g_states.find(state.playerId);
            if (it == g_states.end() || state.revision >= it->second.revision) {
                g_states[state.playerId] = state;
                scheduleApply(state.playerId);
            }
        }
        return;
    }
    if (opcode == Opcode::CancelPreview) {
        std::uint16_t playerId = 0;
        if (reader.read(playerId) && reader.empty() && g_preview && g_preview->playerId == playerId) {
            g_preview.reset();
            scheduleApply(playerId);
        }
    }
}

SampPacketR3* __fastcall hookReceive(void* self, void*) {
    auto* packet = g_originalReceive(self);
    if (packet) processPacket(*packet);
    return packet;
}

void __cdecl hookGameProcess() {
    g_originalGameProcess();
    ++g_gameFrame;
    applyPending();
}

bool hashPackage(std::string& error) {
    const auto root = gameRoot();
    wchar_t ownModule[MAX_PATH]{};
    GetModuleFileNameW(g_module, ownModule, static_cast<DWORD>(std::size(ownModule)));
    const std::array<std::filesystem::path, drip::protocol::kFileCount> paths = {
        std::filesystem::path(ownModule),
        root / L"modloader" / L"omp-drip" / L"models" / L"player.img",
        root / L"modloader" / L"omp-drip" / L"data" / L"clothes.dat",
        root / L"modloader" / L"omp-drip" / L"data" / L"shopping.dat",
        root / L"omp-drip" / L"catalog.bin",
    };
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (!sha256File(paths[index].wstring(), g_fileHashes[index], error)) {
            error += ": " + paths[index].string();
            return false;
        }
    }
    return sha256File((root / L"omp-drip" / L"manifest.json").wstring(), g_manifestHash, error);
}

std::vector<std::uint8_t> helloPacket() {
    Writer writer(Opcode::ClientHello);
    writer.appendBytes(kBuildGuid.data(), kBuildGuid.size());
    writer.appendBytes(g_manifestHash.data(), g_manifestHash.size());
    for (const auto& hash : g_fileHashes) writer.appendBytes(hash.data(), hash.size());
    writer.append(static_cast<std::uint8_t>(g_clientStatus));
    std::array<char, drip::protocol::kConflictNameSize> conflict{};
    std::memcpy(conflict.data(), g_conflictModule.data(), std::min(g_conflictModule.size(), conflict.size() - 1));
    writer.appendBytes(conflict.data(), conflict.size());
    return writer.finish();
}

bool sendHello() {
    auto* game = netGame();
    if (!game || !readable(game) || game->gameState != kConnectedState || !game->rakClient) return false;
    const auto packet = helloPacket();
    if (packet.empty()) return false;
    const bool sent = game->rakClient->Send(reinterpret_cast<const char*>(packet.data()),
        static_cast<int>(packet.size()), RakNet::HIGH_PRIORITY, RakNet::RELIABLE_ORDERED, 4);
    if (sent) {
        g_helloSent = true;
        log("Handshake enviado ao servidor.");
    }
    return sent;
}

bool createHook(void* target, void* detour, void** original, const char* name) {
    const auto status = MH_CreateHook(target, detour, original);
    if (status != MH_OK || MH_EnableHook(target) != MH_OK) {
        log(std::string("Falha ao instalar hook: ") + name);
        return false;
    }
    return true;
}

bool installHooks() {
    if (MH_Initialize() != MH_OK) {
        log("MH_Initialize falhou.");
        return false;
    }
    const auto setConflict = inspectHookTarget(reinterpret_cast<void*>(kSetTextureAndModel));
    const auto rebuildConflict = inspectHookTarget(reinterpret_cast<void*>(kRebuildPlayer));
    if (setConflict.hooked || rebuildConflict.hooked) {
        g_clientStatus = drip::protocol::RejectReason::HookConflict;
        g_conflictModule = setConflict.hooked ? setConflict.module : rebuildConflict.module;
        log("Conflito de roupas detectado em " + g_conflictModule);
        return false;
    }

    auto* game = netGame();
    if (!game || !readable(game) || !game->rakClient || !readable(game->rakClient, sizeof(void*))) {
        log("RakClient indisponivel durante a instalacao dos hooks.");
        return false;
    }
    auto** rakVtable = *reinterpret_cast<void***>(game->rakClient);
    if (!readable(rakVtable, sizeof(void*) * (kRakClientReceiveVtableIndex + 1))
        || !readable(rakVtable[kRakClientReceiveVtableIndex])) {
        log("Vtable do RakClient R3 invalida.");
        return false;
    }
    void* receiveTarget = rakVtable[kRakClientReceiveVtableIndex];

    if (!createHook(reinterpret_cast<void*>(kSetTextureAndModel), reinterpret_cast<void*>(&hookSetTexture),
            reinterpret_cast<void**>(&g_originalSetTexture), "SetTextureAndModel")
        || !createHook(reinterpret_cast<void*>(kRebuildPlayer), reinterpret_cast<void*>(&hookRebuild),
            reinterpret_cast<void**>(&g_originalRebuild), "RebuildPlayer")
        || !createHook(reinterpret_cast<void*>(kGameProcess), reinterpret_cast<void*>(&hookGameProcess),
            reinterpret_cast<void**>(&g_originalGameProcess), "CGame::Process")
        || !createHook(receiveTarget,
            reinterpret_cast<void*>(&hookReceive), reinterpret_cast<void**>(&g_originalReceive), "RakPeer::Receive")) {
        g_clientStatus = drip::protocol::RejectReason::HookConflict;
        g_conflictModule = "hook-incompativel";
        return false;
    }
    return true;
}

DWORD WINAPI workerThread(void*) {
    bool duplicate = false;
    g_ownership = claimBuildOwnership(duplicate);
    if (duplicate) {
        g_clientStatus = drip::protocol::RejectReason::DuplicateClient;
        g_conflictModule = "omp-drip.asi";
        log("Copia duplicada detectada.");
    }

    while (g_running && !(g_sampModule = GetModuleHandleW(L"samp.dll"))) Sleep(100);
    while (g_running && !netGame()) Sleep(100);
    if (!g_running) return 0;

    std::string error;
    if (!hashPackage(error)) {
        g_clientStatus = drip::protocol::RejectReason::InvalidPackage;
        log(error);
    }
    if (!g_catalog.load((gameRoot() / L"omp-drip" / L"catalog.bin").wstring(), error)) {
        g_clientStatus = drip::protocol::RejectReason::InvalidPackage;
        log(error);
    }

    if (!duplicate && g_clientStatus == drip::protocol::RejectReason::None) installHooks();

    RakNet::RakClientInterface* previousClient = nullptr;
    bool wasConnected = false;
    while (g_running) {
        auto* game = netGame();
        const bool connected = game && readable(game) && game->gameState == kConnectedState && game->rakClient;
        if (!connected) {
            if (wasConnected) {
                g_helloSent = false;
                g_accepted = false;
                previousClient = nullptr;
                log("Conexao encerrada; handshake sera renovado no reconnect.");
            }
            wasConnected = false;
        } else {
            if (!wasConnected || game->rakClient != previousClient) {
                g_helloSent = false;
                g_accepted = false;
                previousClient = game->rakClient;
            }
            if (!g_helloSent) sendHello();
            wasConnected = true;
        }
        Sleep(200);
    }
    if (g_clientStatus == drip::protocol::RejectReason::HookConflict) {
        std::string message = "omp-drip detectou outro modulo alterando as roupas: " + g_conflictModule
            + ".\n\nDesative esse modulo e reinicie o jogo.";
        MessageBoxA(nullptr, message.c_str(), "omp-drip - conflito", MB_ICONERROR | MB_OK);
    }
    return 0;
}

} // namespace

} // namespace drip::client

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    using namespace drip::client;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, workerThread, nullptr, 0, nullptr)) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = false;
        if (g_ownership) CloseHandle(g_ownership);
        MH_Uninitialize();
    }
    return TRUE;
}
