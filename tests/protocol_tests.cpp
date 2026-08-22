#include "protocol.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

using namespace drip::protocol;

int main() {
    AppearanceState input;
    input.playerId = 42;
    input.revision = 77;
    input.items[0] = 0x12345678U;
    input.items[17] = 0xFFFFFFFFU;
    input.muscle = 999;
    input.fat = 123;

    auto packet = makeStatePacket(Opcode::FullState, input);
    assert(!packet.empty());
    assert(packet.front() == kPacketId);

    Reader reader(packet.data(), packet.size(), true);
    Opcode opcode{};
    assert(reader.readHeader(opcode));
    assert(opcode == Opcode::FullState);
    AppearanceState output;
    assert(readState(reader, output));
    assert(output.playerId == input.playerId);
    assert(output.revision == input.revision);
    assert(output.items == input.items);
    assert(output.muscle == input.muscle);
    assert(output.fat == input.fat);

    packet.pop_back();
    Reader truncated(packet.data(), packet.size(), true);
    assert(!truncated.readHeader(opcode));

    auto invalid = makeStatePacket(Opcode::StateUpdate, input);
    invalid[6] = 0xFE;
    Reader badOpcode(invalid.data(), invalid.size(), true);
    assert(!badOpcode.readHeader(opcode));

    auto badVersion = makeStatePacket(Opcode::StateUpdate, input);
    badVersion[4] = 2;
    Reader versionReader(badVersion.data(), badVersion.size(), true);
    assert(!versionReader.readHeader(opcode));
    return 0;
}

