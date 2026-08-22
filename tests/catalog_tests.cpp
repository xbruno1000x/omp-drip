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

void record(std::vector<std::uint8_t>& bytes, std::uint32_t id) {
    append(bytes, id);
    bytes.push_back(0);
    bytes.push_back(5);
    bytes.push_back(5);
    bytes.push_back(0);
    bytes.insert(bytes.end(), {'s', 'h', 'i', 'r', 't', 't', 'o', 'r', 's', 'o'});
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

    error.clear();
    assert(!load(catalog(2), parsed, error));

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
