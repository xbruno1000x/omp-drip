#include "conflict_detector.hpp"

#include <Windows.h>

#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>

namespace {

void knownTarget() {}

} // namespace

int main() {
    bool firstDuplicate = true;
    bool secondDuplicate = false;
    HANDLE first = drip::client::claimBuildOwnership(firstDuplicate);
    HANDLE second = drip::client::claimBuildOwnership(secondDuplicate);
    assert(first && second);
    assert(!firstDuplicate);
    assert(secondDuplicate);

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
