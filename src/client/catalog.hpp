#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace drip::client {

struct CatalogItem {
    std::uint32_t id = 0;
    std::uint8_t slot = 0;
    std::string texture;
    std::string model;
};

class Catalog final {
public:
    bool load(const std::wstring& path, std::string& error);
    const CatalogItem* find(std::uint32_t id) const;
    bool empty() const { return items_.empty(); }

private:
    std::unordered_map<std::uint32_t, CatalogItem> items_;
};

} // namespace drip::client

