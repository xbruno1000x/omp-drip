#include "package_paths.hpp"

#include <cassert>
#include <chrono>
#include <fstream>
#include <string>

int main() {
    const auto root = std::filesystem::temp_directory_path()
        / ("omp-drip-package-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "models");
    std::ofstream(root / "models" / "player.img") << "vanilla";
    assert(drip::client::packageAssetRoot(root) == root);

    // An unrelated Mod Loader mod must not change the selected assets.
    std::filesystem::create_directories(root / "modloader" / "another-mod");
    assert(drip::client::packageAssetRoot(root) == root);

    const auto customRoot = root / "modloader" / "omp-drip";
    std::filesystem::create_directories(customRoot);
    assert(drip::client::packageAssetRoot(root) == customRoot);
    // Do not fall back to the existing vanilla player.img for a partial package.
    assert(!std::filesystem::exists(drip::client::packageAssetRoot(root) / "models" / "player.img"));
    std::filesystem::create_directories(customRoot / "models");
    std::ofstream(customRoot / "models" / "player.img") << "custom";
    assert(drip::client::packageAssetRoot(root) == customRoot);

    std::filesystem::remove_all(root);
}
