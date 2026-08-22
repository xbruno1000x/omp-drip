#include "file_hash.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cstdio>
#include <sstream>
#include <vector>

namespace drip::client {

bool sha256File(const std::wstring& path, Sha256& output, std::string& error) {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) {
        error = "nao foi possivel abrir arquivo exigido";
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0;
    DWORD resultSize = 0;
    std::vector<std::uint8_t> hashObject;
    bool success = false;

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0
        && BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize), &resultSize, 0) >= 0) {
        hashObject.resize(objectSize);
        if (BCryptCreateHash(algorithm, &hash, hashObject.data(), objectSize, nullptr, 0, 0) >= 0) {
            std::array<std::uint8_t, 64 * 1024> buffer{};
            while (true) {
                const auto count = std::fread(buffer.data(), 1, buffer.size(), file);
                if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) break;
                if (count < buffer.size()) {
                    if (!std::ferror(file)
                        && BCryptFinishHash(hash, output.data(), static_cast<ULONG>(output.size()), 0) >= 0) {
                        success = true;
                    }
                    break;
                }
            }
        }
    }

    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    std::fclose(file);
    if (!success) error = "falha ao calcular SHA-256";
    return success;
}

std::string hexHash(const Sha256& hash) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string output(hash.size() * 2, '0');
    for (std::size_t i = 0; i < hash.size(); ++i) {
        output[i * 2] = digits[hash[i] >> 4U];
        output[i * 2 + 1] = digits[hash[i] & 0xFU];
    }
    return output;
}

} // namespace drip::client
