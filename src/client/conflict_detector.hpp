#pragma once

#include <Windows.h>

#include <string>

namespace drip::client {

struct ConflictResult {
    bool duplicate = false;
    bool hooked = false;
    std::string module;
};

HANDLE claimBuildOwnership(bool& duplicate);
ConflictResult inspectHookTarget(void* address);

} // namespace drip::client

