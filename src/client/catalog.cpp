#include "catalog.hpp"

#include <array>
#include <fstream>
#include <limits>
#include <type_traits>
#include <vector>

namespace drip::client {

namespace {

template <typename T>
bool readLittle(std::istream& stream, T& value) {
    static_assert(std::is_integral_v<T>);
    std::array<unsigned char, sizeof(T)> bytes{};
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) return false;
    std::make_unsigned_t<T> raw = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i) raw |= std::make_unsigned_t<T>(bytes[i]) << (i * 8U);
    value = static_cast<T>(raw);
    return true;
}

} // namespace

bool Catalog::load(const std::wstring& path, std::string& error) {
    items_.clear();
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "catalog.bin nao encontrado";
        return false;
    }

    char magic[4]{};
    std::uint16_t version = 0;
    std::uint32_t count = 0;
    if (!stream.read(magic, sizeof(magic)) || std::string(magic, sizeof(magic)) != "HCAT"
        || !readLittle(stream, version) || version != 1 || !readLittle(stream, count)
        || count == 0 || count > 100'000) {
        error = "cabecalho de catalogo invalido";
        return false;
    }

    for (std::uint32_t index = 0; index < count; ++index) {
        CatalogItem item;
        std::uint8_t textureLength = 0;
        std::uint8_t modelLength = 0;
        std::uint8_t flags = 0;
        if (!readLittle(stream, item.id) || !readLittle(stream, item.slot)
            || !readLittle(stream, textureLength) || !readLittle(stream, modelLength)
            || !readLittle(stream, flags) || flags != 0 || item.slot >= 18 || textureLength == 0 || modelLength == 0
            || textureLength > 31 || modelLength > 31) {
            error = "registro de catalogo invalido";
            items_.clear();
            return false;
        }
        item.texture.resize(textureLength);
        item.model.resize(modelLength);
        if (!stream.read(item.texture.data(), textureLength) || !stream.read(item.model.data(), modelLength)) {
            error = "catalogo truncado";
            items_.clear();
            return false;
        }
        if (!items_.emplace(item.id, std::move(item)).second) {
            error = "ID duplicado no catalogo";
            items_.clear();
            return false;
        }
    }
    if (stream.peek() != std::char_traits<char>::eof()) {
        error = "dados excedentes no catalogo";
        items_.clear();
        return false;
    }
    return true;
}

const CatalogItem* Catalog::find(std::uint32_t id) const {
    const auto it = items_.find(id);
    return it == items_.end() ? nullptr : &it->second;
}

} // namespace drip::client
