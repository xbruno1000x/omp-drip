#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace drip::protocol {

constexpr std::uint8_t kPacketId = 0xFA;
constexpr std::uint16_t kVersion = 1;
constexpr std::size_t kSlotCount = 18;
constexpr std::size_t kHashSize = 32;
constexpr std::size_t kFileCount = 5;
constexpr std::size_t kBuildGuidSize = 16;
constexpr std::size_t kConflictNameSize = 64;
constexpr std::size_t kMaxPacketBytes = 512;
constexpr char kMagic[3] = {'O', 'D', 'R'};

enum class Opcode : std::uint8_t {
    ClientHello = 1,
    ServerAccepted = 2,
    ServerRejected = 3,
    FullState = 4,
    StateUpdate = 5,
    Preview = 6,
    CancelPreview = 7,
    ResyncRequest = 8,
};

enum class RejectReason : std::uint8_t {
    None = 0,
    MissingClient = 1,
    ProtocolMismatch = 2,
    InvalidPackage = 3,
    MalformedPacket = 4,
    DuplicateClient = 5,
    HookConflict = 6,
};

struct AppearanceState {
    std::uint16_t playerId = 0;
    std::uint32_t revision = 0;
    std::array<std::uint32_t, kSlotCount> items{};
    std::uint16_t muscle = 0;
    std::uint16_t fat = 0;
};

struct ClientHello {
    std::array<std::uint8_t, kBuildGuidSize> buildGuid{};
    std::array<std::uint8_t, kHashSize> manifestHash{};
    std::array<std::array<std::uint8_t, kHashSize>, kFileCount> fileHashes{};
    RejectReason clientStatus = RejectReason::None;
    std::array<char, kConflictNameSize> conflictModule{};
};

class Writer final {
public:
    explicit Writer(Opcode opcode) : opcode_(opcode) {
        bytes_.reserve(128);
        bytes_.push_back(kPacketId);
        bytes_.insert(bytes_.end(), std::begin(kMagic), std::end(kMagic));
        append(kVersion);
        append(static_cast<std::uint8_t>(opcode));
        payloadSizeOffset_ = bytes_.size();
        append<std::uint16_t>(0);
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    void append(T value) {
        using U = std::make_unsigned_t<T>;
        U raw = static_cast<U>(value);
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            bytes_.push_back(static_cast<std::uint8_t>((raw >> (i * 8U)) & 0xFFU));
        }
    }

    void appendBytes(const void* data, std::size_t size) {
        const auto* begin = static_cast<const std::uint8_t*>(data);
        bytes_.insert(bytes_.end(), begin, begin + size);
    }

    std::vector<std::uint8_t> finish() {
        const std::size_t payload = bytes_.size() - headerSize();
        if (payload > std::numeric_limits<std::uint16_t>::max() || bytes_.size() > kMaxPacketBytes) {
            return {};
        }
        bytes_[payloadSizeOffset_] = static_cast<std::uint8_t>(payload & 0xFFU);
        bytes_[payloadSizeOffset_ + 1] = static_cast<std::uint8_t>((payload >> 8U) & 0xFFU);
        return bytes_;
    }

    static constexpr std::size_t headerSize() { return 1 + 3 + 2 + 1 + 2; }

private:
    Opcode opcode_;
    std::vector<std::uint8_t> bytes_;
    std::size_t payloadSizeOffset_ = 0;
};

class Reader final {
public:
    Reader(const std::uint8_t* data, std::size_t size, bool includesPacketId)
        : data_(data), size_(size), offset_(includesPacketId ? 1U : 0U) {}

    bool readHeader(Opcode& opcode) {
        if (headerRead_ || size_ > kMaxPacketBytes) return false;
        if (offset_ + 8U > size_) return false;
        if (std::memcmp(data_ + offset_, kMagic, sizeof(kMagic)) != 0) return false;
        offset_ += sizeof(kMagic);
        std::uint16_t version = 0;
        std::uint8_t rawOpcode = 0;
        std::uint16_t payloadSize = 0;
        if (!read(version) || !read(rawOpcode) || !read(payloadSize)) return false;
        if (version != kVersion || payloadSize != remaining()) return false;
        opcode = static_cast<Opcode>(rawOpcode);
        headerRead_ = true;
        return isKnownOpcode(opcode);
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    bool read(T& value) {
        if (remaining() < sizeof(T)) return false;
        using U = std::make_unsigned_t<T>;
        U raw = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i) {
            raw |= static_cast<U>(data_[offset_ + i]) << (i * 8U);
        }
        value = static_cast<T>(raw);
        offset_ += sizeof(T);
        return true;
    }

    bool readBytes(void* output, std::size_t size) {
        if (remaining() < size) return false;
        std::memcpy(output, data_ + offset_, size);
        offset_ += size;
        return true;
    }

    std::size_t remaining() const { return size_ >= offset_ ? size_ - offset_ : 0; }
    bool empty() const { return remaining() == 0; }

private:
    static bool isKnownOpcode(Opcode value) {
        const auto raw = static_cast<std::uint8_t>(value);
        return raw >= static_cast<std::uint8_t>(Opcode::ClientHello)
            && raw <= static_cast<std::uint8_t>(Opcode::ResyncRequest);
    }

    const std::uint8_t* data_ = nullptr;
    std::size_t size_ = 0;
    std::size_t offset_ = 0;
    bool headerRead_ = false;
};

inline void writeState(Writer& writer, const AppearanceState& state) {
    writer.append(state.playerId);
    writer.append(state.revision);
    for (const auto item : state.items) writer.append(item);
    writer.append(state.muscle);
    writer.append(state.fat);
}

inline bool readState(Reader& reader, AppearanceState& state) {
    if (!reader.read(state.playerId) || !reader.read(state.revision)) return false;
    for (auto& item : state.items) {
        if (!reader.read(item)) return false;
    }
    return reader.read(state.muscle) && reader.read(state.fat) && reader.empty()
        && state.muscle <= 1000 && state.fat <= 1000;
}

inline std::vector<std::uint8_t> makeStatePacket(Opcode opcode, const AppearanceState& state) {
    Writer writer(opcode);
    writeState(writer, state);
    return writer.finish();
}

} // namespace drip::protocol
