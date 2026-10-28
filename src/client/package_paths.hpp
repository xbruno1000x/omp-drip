#pragma once

#include <filesystem>

namespace drip::client {

// Custom packages take precedence as a whole. An incomplete custom package
// must fail validation rather than silently mixing custom and vanilla files.
inline std::filesystem::path packageAssetRoot(const std::filesystem::path& gameRoot) {
    const auto customRoot = gameRoot / L"modloader" / L"omp-drip";
    return std::filesystem::exists(customRoot) ? customRoot : gameRoot;
}

} // namespace drip::client
