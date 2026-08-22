#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace drip::client {

using Sha256 = std::array<std::uint8_t, 32>;

bool sha256File(const std::wstring& path, Sha256& output, std::string& error);
std::string hexHash(const Sha256& hash);

} // namespace drip::client

