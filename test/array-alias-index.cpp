#include <array>
#include <cstdint>
#include <cstdio>

#include "ArrayAliasIndex.h"

int main() {
  SArrayAliasIndex dut;
  struct Entry {
    bool valid = false;
    unsigned bits = 0;
  };
  std::array<Entry, 2> expected;

  dut.set_allocValid(0);
  dut.set_allocIdx(0);
  dut.set_eventValid(0);
  dut.set_eventIdx(0);
  dut.set_eventPosition(0);
  // Initialize both valid bits without relying on uninitialized register data.
  for (unsigned idx = 0; idx < 2; ++idx) {
    dut.set_allocValid(1);
    dut.set_allocIdx(idx);
    dut.step();
    dut.set_allocValid(0);
    dut.step();
  }

  unsigned cycle = 0;
  auto check = [&]() {
    const std::array<unsigned, 2> valid = {dut.get_valid_0(), dut.get_valid_1()};
    const std::array<unsigned, 2> bits = {dut.get_bits_0(), dut.get_bits_1()};
    for (unsigned idx = 0; idx < 2; ++idx) {
      if (valid[idx] != expected[idx].valid ||
          (expected[idx].valid && bits[idx] != expected[idx].bits)) {
        std::fprintf(stderr, "cycle %u entry %u: got valid=%u bits=%u; expected valid=%u bits=%u\n",
                     cycle, idx, valid[idx], bits[idx], unsigned(expected[idx].valid), expected[idx].bits);
        return false;
      }
    }
    return true;
  };
  auto apply = [&](bool alloc, unsigned allocIdx, bool event, unsigned eventIdx, unsigned position) {
    if (alloc) expected[allocIdx].valid = false;
    if (event && (!expected[eventIdx].valid || position < expected[eventIdx].bits)) {
      expected[eventIdx] = {true, position};
    }
    dut.set_allocValid(alloc);
    dut.set_allocIdx(allocIdx);
    dut.set_eventValid(event);
    dut.set_eventIdx(eventIdx);
    dut.set_eventPosition(position);
    dut.step();
    // GSIM evaluates next state, then commits it at the start of the next step.
    dut.set_allocValid(0);
    dut.set_eventValid(0);
    dut.step();
    ++cycle;
    return check();
  };

  if (!check()) return 1;
  if (!apply(false, 0, true, 0, 17)) return 2;
  if (!apply(false, 0, true, 0, 24)) return 3; // Later events retain the earlier position.
  if (!apply(false, 0, true, 0, 9)) return 4;
  if (!apply(false, 0, true, 1, 25)) return 5;
  if (!apply(false, 0, true, 1, 25)) return 6;
  if (!apply(true, 0, true, 0, 31)) return 7; // Allocation clears valid before the read.
  if (!apply(true, 1, true, 0, 18)) return 8;
  if (!apply(false, 0, false, 1, 0)) return 9;

  uint32_t random = 0x4a39b70d;
  auto next = [&]() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return random;
  };
  for (unsigned i = 0; i < 512; ++i) {
    const bool alloc = (next() & 3) == 0;
    const unsigned allocIdx = next() & 1;
    const bool event = (next() & 3) != 0;
    const unsigned eventIdx = next() & 1;
    const unsigned position = next() & 31;
    if (!apply(alloc, allocIdx, event, eventIdx, position)) return 10;
  }
  std::puts("array alias index: PASS");
  return 0;
}
