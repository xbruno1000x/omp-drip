#include "conflict_detector.hpp"

#include <Psapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace drip::client {

namespace {

constexpr wchar_t kOwnershipName[] = L"Local\\omp-drip-8DF09E6A-17BB-4BDF-9EAE-D21B93AF134C";

std::string moduleNameForAddress(const void* address) {
    HMODULE modules[1024]{};
    DWORD needed = 0;
    const HANDLE process = GetCurrentProcess();
    if (!EnumProcessModules(process, modules, sizeof(modules), &needed)) return "modulo-desconhecido";
    const auto target = reinterpret_cast<std::uintptr_t>(address);
    const auto count = std::min<std::size_t>(needed / sizeof(HMODULE), std::size(modules));
    for (std::size_t i = 0; i < count; ++i) {
        MODULEINFO info{};
        if (!GetModuleInformation(process, modules[i], &info, sizeof(info))) continue;
        const auto begin = reinterpret_cast<std::uintptr_t>(info.lpBaseOfDll);
        if (target >= begin && target < begin + info.SizeOfImage) {
            char name[MAX_PATH]{};
            if (GetModuleBaseNameA(process, modules[i], name, sizeof(name))) return name;
        }
    }
    return "memoria-anonima";
}

bool readableRange(const void* address, std::size_t size) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!address || !VirtualQuery(address, &memory, sizeof(memory)) || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return begin + size <= end;
}

const void* detourDestination(const std::uint8_t* code) {
    if (code[0] == 0xE9 || code[0] == 0xE8) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, code + 1, sizeof(displacement));
        return code + 5 + displacement;
    }
    if (code[0] == 0x68 && code[5] == 0xC3) {
        std::uint32_t absolute = 0;
        std::memcpy(&absolute, code + 1, sizeof(absolute));
        return reinterpret_cast<const void*>(absolute);
    }
    if (code[0] == 0xFF && code[1] == 0x25) {
        std::uint32_t slot = 0;
        std::memcpy(&slot, code + 2, sizeof(slot));
        const auto* pointer = reinterpret_cast<void* const*>(slot);
        if (slot && readableRange(pointer, sizeof(*pointer))) return *pointer;
    }
    return nullptr;
}

} // namespace

HANDLE claimBuildOwnership(bool& duplicate) {
    // Duplicate ASIs matter within one game process, not across separate games
    // or test processes running in the same Windows session.
    const auto name = std::wstring(kOwnershipName) + L"-" + std::to_wstring(GetCurrentProcessId());
    SetLastError(ERROR_SUCCESS);
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1, name.c_str());
    duplicate = mapping && GetLastError() == ERROR_ALREADY_EXISTS;
    return mapping;
}

ConflictResult inspectHookTarget(void* address) {
    ConflictResult result;
    MEMORY_BASIC_INFORMATION memory{};
    if (!address || !VirtualQuery(address, &memory, sizeof(memory)) || memory.State != MEM_COMMIT
        || !readableRange(address, 6)) {
        result.hooked = true;
        result.module = "endereco-invalido";
        return result;
    }

    const auto* code = static_cast<const std::uint8_t*>(address);
    if (const void* destination = detourDestination(code)) {
        result.hooked = true;
        result.module = moduleNameForAddress(destination);
    }
    return result;
}

} // namespace drip::client
