#include <sdk.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <Server/Components/Pawn/Impl/pawn_impl.hpp>

#include <bitstream.hpp>

#include "manifest.hpp"
#include "protocol.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using drip::protocol::AppearanceState;
using drip::protocol::ClientHello;
using drip::protocol::Opcode;
using drip::protocol::Reader;
using drip::protocol::RejectReason;
using drip::protocol::Writer;

constexpr std::uint32_t kHandshakeTimeoutMs = 10'000;
const Colour kInfoColour(0xFF, 0xAA, 0x33);
const Colour kErrorColour(0xFF, 0x66, 0x66);

struct ClientSession {
    bool ready = false;
    bool rejected = false;
    RejectReason rejectReason = RejectReason::None;
    std::uint32_t deadline = 0;
};

class OmpDripComponent;
OmpDripComponent* g_component = nullptr;

class OmpDripComponent final
    : public IComponent
    , public PawnEventHandler
    , public PlayerConnectEventHandler
    , public PlayerSpawnEventHandler
    , public PlayerStreamEventHandler
    , public CoreEventHandler
    , public SingleNetworkInEventHandler {
public:
    PROVIDE_UID(UID(0x4F4D504452495031));

    ~OmpDripComponent() override {
        if (pawn_) pawn_->getEventDispatcher().removeEventHandler(this);
        if (core_) {
            core_->removePerPacketInEventHandler<drip::protocol::kPacketId>(this);
            core_->getEventDispatcher().removeEventHandler(this);
            auto& players = core_->getPlayers();
            players.getPlayerConnectDispatcher().removeEventHandler(this);
            players.getPlayerSpawnDispatcher().removeEventHandler(this);
            players.getPlayerStreamDispatcher().removeEventHandler(this);
        }
        if (g_component == this) g_component = nullptr;
    }

    StringView componentName() const override { return "omp-drip"; }
    SemanticVersion componentVersion() const override { return SemanticVersion(1, 0, 0, 0); }

    void onLoad(ICore* core) override {
        core_ = core;
        g_component = this;
        core_->getEventDispatcher().addEventHandler(this);
        auto& players = core_->getPlayers();
        players.getPlayerConnectDispatcher().addEventHandler(this);
        players.getPlayerSpawnDispatcher().addEventHandler(this);
        players.getPlayerStreamDispatcher().addEventHandler(this);
        core_->addPerPacketInEventHandler<drip::protocol::kPacketId>(this);

        if (drip::manifest::kConfigured) {
            core_->printLn("[omp-drip] Protocolo v%u ativo; handshake obrigatorio em %u ms.",
                drip::protocol::kVersion, kHandshakeTimeoutMs);
        } else {
            core_->logLn(LogLevel::Warning,
                "[omp-drip] Manifesto ainda nao foi gerado; handshake obrigatorio desativado.");
        }
    }

    void onInit(IComponentList* components) override {
        pawn_ = components->queryComponent<IPawnComponent>();
        if (!pawn_) {
            core_->logLn(LogLevel::Error, "[omp-drip] Componente Pawn nao encontrado.");
            return;
        }
        setAmxFunctions(pawn_->getAmxFunctions());
        pawn_->getEventDispatcher().addEventHandler(this);
    }

    void onFree(IComponent* component) override {
        if (component == pawn_) {
            pawn_ = nullptr;
            setAmxFunctions();
        }
    }

    void onAmxLoad(IPawnScript& script) override;
    void onAmxUnload(IPawnScript&) override {}

    void onPlayerConnect(IPlayer& player) override {
        AppearanceState state;
        state.playerId = static_cast<std::uint16_t>(player.getID());
        appearances_[player.getID()] = state;

        ClientSession session;
        session.ready = player.isBot() || !drip::manifest::kConfigured;
        session.deadline = core_->getTickCount() + kHandshakeTimeoutMs;
        sessions_[player.getID()] = session;

        if (drip::manifest::kConfigured && !player.isBot()) {
            player.sendClientMessage(kInfoColour,
                "[Roupas] Validando o pacote omp-drip antes do login...");
        }
    }

    void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason) override {
        sessions_.erase(player.getID());
        appearances_.erase(player.getID());
    }

    void onPlayerSpawn(IPlayer& player) override {
        if (isReady(player.getID())) sendState(player, stateFor(player.getID()), Opcode::FullState);
    }

    void onPlayerStreamIn(IPlayer& player, IPlayer& forPlayer) override {
        if (isReady(forPlayer.getID())) sendState(forPlayer, stateFor(player.getID()), Opcode::FullState);
    }

    void onTick(Microseconds, TimePoint) override {
        if (!drip::manifest::kConfigured) return;
        const auto now = core_->getTickCount();
        std::vector<int> expired;
        for (const auto& [playerId, session] : sessions_) {
            if (!session.ready && static_cast<std::int32_t>(now - session.deadline) >= 0) {
                expired.push_back(playerId);
            }
        }
        for (const int playerId : expired) {
            auto* player = core_->getPlayers().get(playerId);
            if (!player || player->isBot()) continue;
            const auto it = sessions_.find(playerId);
            const RejectReason reason = it == sessions_.end() || !it->second.rejected
                ? RejectReason::MissingClient : it->second.rejectReason;
            player->sendClientMessage(kErrorColour, rejectionMessage(reason));
            notifyRejected(playerId, reason);
            player->kick();
        }
    }

    bool onReceive(IPlayer& peer, NetworkBitStream& bitStream) override {
        const int totalBytes = bitStream.GetNumberOfBytesUsed();
        if (totalBytes <= 1 || totalBytes > static_cast<int>(drip::protocol::kMaxPacketBytes)) {
            reject(peer, RejectReason::MalformedPacket);
            return false;
        }
        Reader reader(bitStream.GetData() + 1, static_cast<std::size_t>(totalBytes - 1), false);
        Opcode opcode{};
        if (!reader.readHeader(opcode)) {
            reject(peer, RejectReason::MalformedPacket);
            return false;
        }

        if (opcode == Opcode::ClientHello) return handleHello(peer, reader);
        if (opcode == Opcode::ResyncRequest && reader.empty()) {
            if (isReady(peer.getID())) resync(peer);
            return false;
        }
        reject(peer, RejectReason::MalformedPacket);
        return false;
    }

    bool isReady(int playerId) const {
        const auto it = sessions_.find(playerId);
        return it != sessions_.end() && it->second.ready;
    }

    bool setAppearance(IPlayer& player, const std::array<std::uint32_t, drip::protocol::kSlotCount>& items,
        std::uint16_t muscle, std::uint16_t fat) {
        auto& state = stateFor(player.getID());
        if (state.items == items && state.muscle == muscle && state.fat == fat) return true;
        state.items = items;
        state.muscle = muscle;
        state.fat = fat;
        if (++state.revision == 0) state.revision = 1;
        broadcastState(player, state, Opcode::StateUpdate);
        return true;
    }

    bool preview(IPlayer& player, int slot, std::uint32_t itemId) {
        if (slot < 0 || slot >= static_cast<int>(drip::protocol::kSlotCount) || !isReady(player.getID())) return false;
        auto previewState = stateFor(player.getID());
        previewState.items[static_cast<std::size_t>(slot)] = itemId;
        sendState(player, previewState, Opcode::Preview);
        return true;
    }

    bool clearPreview(IPlayer& player) {
        if (!isReady(player.getID())) return false;
        Writer writer(Opcode::CancelPreview);
        writer.append(static_cast<std::uint16_t>(player.getID()));
        return sendRaw(player, writer.finish());
    }

    void resync(IPlayer& player) {
        sendState(player, stateFor(player.getID()), Opcode::FullState);
        for (IPlayer* candidate : core_->getPlayers().entries()) {
            if (candidate == &player) continue;
            const auto& viewers = candidate->streamedForPlayers();
            if (viewers.find(&player) != viewers.end()) {
                sendState(player, stateFor(candidate->getID()), Opcode::FullState);
            }
        }
    }

    void free() override { delete this; }
    void reset() override {
        for (auto& [id, state] : appearances_) {
            state = AppearanceState{};
            state.playerId = static_cast<std::uint16_t>(id);
        }
    }

private:
    static StringView rejectionMessage(RejectReason reason) {
        switch (reason) {
            case RejectReason::ProtocolMismatch:
                return "[Roupas] Versao do omp-drip incompativel. Instale o ZIP oficial atual.";
            case RejectReason::InvalidPackage:
                return "[Roupas] Pacote alterado ou incompleto. Reinstale o ZIP oficial atual.";
            case RejectReason::DuplicateClient:
                return "[Roupas] Duas copias do omp-drip foram carregadas. Remova a duplicata.";
            case RejectReason::HookConflict:
                return "[Roupas] Outro mod esta alterando as roupas do jogador. Desative o modulo informado pelo ASI.";
            case RejectReason::MalformedPacket:
                return "[Roupas] Cliente enviou dados invalidos. Reinstale o ZIP oficial atual.";
            default:
                return "[Roupas] omp-drip.asi nao respondeu. Instale o ZIP oficial para entrar.";
        }
    }

    bool handleHello(IPlayer& peer, Reader& reader) {
        ClientHello hello;
        if (!reader.readBytes(hello.buildGuid.data(), hello.buildGuid.size())
            || !reader.readBytes(hello.manifestHash.data(), hello.manifestHash.size())) {
            reject(peer, RejectReason::MalformedPacket);
            return false;
        }
        for (auto& hash : hello.fileHashes) {
            if (!reader.readBytes(hash.data(), hash.size())) {
                reject(peer, RejectReason::MalformedPacket);
                return false;
            }
        }
        std::uint8_t clientStatus = 0;
        if (!reader.read(clientStatus)
            || !reader.readBytes(hello.conflictModule.data(), hello.conflictModule.size())) {
            reject(peer, RejectReason::MalformedPacket);
            return false;
        }
        hello.clientStatus = static_cast<RejectReason>(clientStatus);
        if (!reader.empty()) {
            reject(peer, RejectReason::MalformedPacket);
            return false;
        }

        if (hello.clientStatus == RejectReason::DuplicateClient
            || hello.clientStatus == RejectReason::HookConflict
            || hello.clientStatus == RejectReason::InvalidPackage) {
            if (hello.clientStatus == RejectReason::HookConflict && hello.conflictModule[0] != '\0') {
                core_->logLn(LogLevel::Warning, "[omp-drip] Player %d reportou conflito com %s.",
                    peer.getID(), hello.conflictModule.data());
            }
            reject(peer, hello.clientStatus);
            return false;
        }

        if (drip::manifest::kConfigured
            && (hello.manifestHash != drip::manifest::kManifestHash
                || hello.fileHashes != drip::manifest::kFileHashes)) {
            reject(peer, RejectReason::InvalidPackage);
            return false;
        }

        auto& session = sessions_[peer.getID()];
        session.ready = true;
        session.rejected = false;
        session.rejectReason = RejectReason::None;
        Writer writer(Opcode::ServerAccepted);
        writer.append(drip::protocol::kVersion);
        sendRaw(peer, writer.finish());
        peer.sendClientMessage(Colour(0x33, 0xAA, 0x33), "[Roupas] Pacote validado com sucesso.");
        notifyReady(peer.getID());
        resync(peer);
        return false;
    }

    void reject(IPlayer& peer, RejectReason reason) {
        auto& session = sessions_[peer.getID()];
        session.ready = false;
        session.rejected = true;
        session.rejectReason = reason;
        Writer writer(Opcode::ServerRejected);
        writer.append(static_cast<std::uint8_t>(reason));
        const auto message = rejectionMessage(reason);
        const auto length = static_cast<std::uint8_t>(std::min<std::size_t>(message.length(), 220));
        writer.append(length);
        writer.appendBytes(message.data(), length);
        sendRaw(peer, writer.finish());
    }

    AppearanceState& stateFor(int playerId) {
        auto [it, inserted] = appearances_.try_emplace(playerId);
        if (inserted) it->second.playerId = static_cast<std::uint16_t>(playerId);
        return it->second;
    }

    bool sendRaw(IPlayer& player, const std::vector<std::uint8_t>& packet) {
        if (packet.empty()) return false;
        return player.sendPacket(
            Span<std::uint8_t>(const_cast<std::uint8_t*>(packet.data()), bytesToBits(static_cast<int>(packet.size()))),
            OrderingChannel_Reliable,
            false);
    }

public:
    IPlayer* player(int playerId) {
        return core_ ? core_->getPlayers().get(playerId) : nullptr;
    }

private:

    bool sendState(IPlayer& recipient, const AppearanceState& state, Opcode opcode) {
        if (!isReady(recipient.getID())) return false;
        return sendRaw(recipient, drip::protocol::makeStatePacket(opcode, state));
    }

    void broadcastState(IPlayer& subject, const AppearanceState& state, Opcode opcode) {
        sendState(subject, state, opcode);
        for (IPlayer* viewer : subject.streamedForPlayers()) sendState(*viewer, state, opcode);
    }

    void notifyReady(int playerId) {
        if (pawn_ && pawn_->mainScript()) {
            pawn_->mainScript()->Call("OnDripClientReady", DefaultReturnValue_True,
                playerId, drip::protocol::kVersion);
        }
    }

    void notifyRejected(int playerId, RejectReason reason) {
        if (pawn_ && pawn_->mainScript()) {
            pawn_->mainScript()->Call("OnDripClientRejected", DefaultReturnValue_True,
                playerId, static_cast<int>(reason));
        }
    }

    ICore* core_ = nullptr;
    IPawnComponent* pawn_ = nullptr;
    std::unordered_map<int, ClientSession> sessions_;
    std::unordered_map<int, AppearanceState> appearances_;
};

cell AMX_NATIVE_CALL Native_IsClientReady(AMX*, const cell* params) {
    return g_component && g_component->isReady(params[1]);
}

cell AMX_NATIVE_CALL Native_SetPlayerAppearance(AMX* amx, const cell* params) {
    if (!g_component || params[0] < 4 * static_cast<cell>(sizeof(cell))) return 0;
    auto* player = g_component->player(params[1]);
    if (!player || params[3] < 0 || params[3] > 1000 || params[4] < 0 || params[4] > 1000) return 0;
    cell* source = nullptr;
    if (amx_GetAddr(amx, params[2], &source) != AMX_ERR_NONE || !source) return 0;
    std::array<std::uint32_t, drip::protocol::kSlotCount> items{};
    for (std::size_t i = 0; i < items.size(); ++i) items[i] = static_cast<std::uint32_t>(source[i]);
    return g_component->setAppearance(*player, items,
        static_cast<std::uint16_t>(params[3]), static_cast<std::uint16_t>(params[4]));
}

cell AMX_NATIVE_CALL Native_SetPlayerPreview(AMX*, const cell* params) {
    if (!g_component) return 0;
    auto* player = g_component->player(params[1]);
    return player && g_component->preview(*player, params[2], static_cast<std::uint32_t>(params[3]));
}

cell AMX_NATIVE_CALL Native_ClearPlayerPreview(AMX*, const cell* params) {
    if (!g_component) return 0;
    auto* player = g_component->player(params[1]);
    return player && g_component->clearPreview(*player);
}

cell AMX_NATIVE_CALL Native_ResyncPlayer(AMX*, const cell* params) {
    if (!g_component) return 0;
    auto* player = g_component->player(params[1]);
    if (!player || !g_component->isReady(player->getID())) return 0;
    g_component->resync(*player);
    return 1;
}

void OmpDripComponent::onAmxLoad(IPawnScript& script) {
    static const AMX_NATIVE_INFO natives[] = {
        {"Drip_IsClientReady", Native_IsClientReady},
        {"Drip_SetPlayerAppearance", Native_SetPlayerAppearance},
        {"Drip_SetPlayerPreview", Native_SetPlayerPreview},
        {"Drip_ClearPlayerPreview", Native_ClearPlayerPreview},
        {"Drip_ResyncPlayer", Native_ResyncPlayer},
    };
    script.Register(natives, static_cast<int>(std::size(natives)));
}

} // namespace

COMPONENT_ENTRY_POINT() {
    return new OmpDripComponent();
}
