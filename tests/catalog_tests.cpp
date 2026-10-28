#include "catalog.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

template <typename T>
void append(std::vector<std::uint8_t>& bytes, T value) {
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        bytes.push_back(static_cast<std::uint8_t>((value >> (index * 8U)) & 0xFFU));
    }
}

void record(std::vector<std::uint8_t>& bytes, std::uint32_t id, std::uint8_t slot = 0,
    const std::string& texture = "shirt", const std::string& model = "torso") {
    append(bytes, id);
    bytes.push_back(slot);
    bytes.push_back(static_cast<std::uint8_t>(texture.size()));
    bytes.push_back(static_cast<std::uint8_t>(model.size()));
    bytes.push_back(0);
    bytes.insert(bytes.end(), texture.begin(), texture.end());
    bytes.insert(bytes.end(), model.begin(), model.end());
}

std::vector<std::uint8_t> catalog(std::uint16_t version = 1, std::uint32_t count = 1) {
    std::vector<std::uint8_t> bytes{'H', 'C', 'A', 'T'};
    append(bytes, version);
    append(bytes, count);
    if (count) record(bytes, 42);
    return bytes;
}

bool load(const std::vector<std::uint8_t>& bytes, drip::client::Catalog& output, std::string& error) {
    static std::uint32_t sequence = 0;
    const auto path = std::filesystem::temp_directory_path()
        / (L"omp-drip-catalog-" + std::to_wstring(++sequence) + L".bin");
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    const bool result = output.load(path.wstring(), error);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    return result;
}

} // namespace

int main() {
    drip::client::Catalog parsed;
    std::string error;

    assert(load(catalog(), parsed, error));
    assert(parsed.find(42) != nullptr);
    assert(std::string(parsed.find(42)->modelName()) == "torso");

    // Texture-only tattoos coexist with clothing in format 2.
    auto mixed = catalog(2, 10);
    for (std::uint8_t slot = 4; slot <= 12; ++slot) record(mixed, 100 + slot, slot, "4weed", "");
    assert(load(mixed, parsed, error));
    assert(parsed.find(42) != nullptr);
    for (std::uint8_t slot = 4; slot <= 12; ++slot) {
        const auto* tattoo = parsed.find(100 + slot);
        assert(tattoo && tattoo->slot == slot && tattoo->texture == "4weed");
        assert(tattoo->modelName() == nullptr);
    }

    // An absent model is invalid in legacy format 1 and outside tattoo slots.
    for (std::uint8_t slot = 0; slot < 18; ++slot) {
        auto invalidTattoo = catalog(1, 2);
        record(invalidTattoo, 43, slot, "4weed", "");
        assert(!load(invalidTattoo, parsed, error));
        assert(parsed.empty());
        if (slot >= 4 && slot <= 12) continue;
        invalidTattoo = catalog(2, 2);
        record(invalidTattoo, 43, slot, "4weed", "");
        assert(!load(invalidTattoo, parsed, error));
    }

    auto emptyTexture = catalog(2, 2);
    record(emptyTexture, 43, 4, "", "");
    assert(!load(emptyTexture, parsed, error));

    mixed.pop_back();
    assert(!load(mixed, parsed, error));
    assert(parsed.empty());

    error.clear();
    assert(!load(catalog(3), parsed, error));

    auto truncated = catalog();
    truncated.pop_back();
    error.clear();
    assert(!load(truncated, parsed, error));

    auto duplicate = catalog(1, 2);
    record(duplicate, 42);
    error.clear();
    assert(!load(duplicate, parsed, error));

    error.clear();
    assert(!load(catalog(1, 0), parsed, error));
    return 0;
}
