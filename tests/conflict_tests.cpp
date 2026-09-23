#include "conflict_detector.hpp"

#include <Windows.h>

#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>

namespace {

void knownTarget() {}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--claim") == 0) {
        bool duplicate = true;
        HANDLE ownership = drip::client::claimBuildOwnership(duplicate);
        const bool valid = ownership && !duplicate;
        if (ownership) CloseHandle(ownership);
        return valid ? 0 : 1;
    }
    bool firstDuplicate = true;
    bool secondDuplicate = false;
    HANDLE first = drip::client::claimBuildOwnership(firstDuplicate);
    HANDLE second = drip::client::claimBuildOwnership(secondDuplicate);
    assert(first && second);
    assert(!firstDuplicate);
    assert(secondDuplicate);

    // A second process may claim ownership while this one holds both handles.
    wchar_t executable[MAX_PATH]{};
    assert(GetModuleFileNameW(nullptr, executable, MAX_PATH));
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --claim";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    assert(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process));
    assert(WaitForSingleObject(process.hProcess, 10000) == WAIT_OBJECT_0);
    DWORD exitCode = 1;
    assert(GetExitCodeProcess(process.hProcess, &exitCode));
    assert(exitCode == 0);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    std::array<std::uint8_t, 16> code{};
    code.fill(0x90);
    assert(!drip::client::inspectHookTarget(code.data()).hooked);

    code[0] = 0xE9;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(&knownTarget) - reinterpret_cast<std::intptr_t>(code.data() + 5));
    std::memcpy(code.data() + 1, &displacement, sizeof(displacement));
    const auto conflict = drip::client::inspectHookTarget(code.data());
    assert(conflict.hooked);
    assert(!conflict.module.empty());
    assert(conflict.module != "modulo-desconhecido");

    CloseHandle(second);
    CloseHandle(first);
    return 0;
}
