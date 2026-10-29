#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "RdwrFeedback.h"

int main() {
  SRdwrFeedback dut;
  dut.set_clock(1);
  std::array<std::array<uint8_t, 4>, 4> memory{};
  std::array<unsigned, 4> previousAddress{};
  std::array<bool, 4> previousRead{};
  unsigned cycle = 0;
  auto tick = [&](bool initialize, bool en, bool write, unsigned address,
                  unsigned data, bool gate) {
    // A rdwr result is undefined after a write/disabled cycle. Only feed
    // it back after a valid read; other writes use defined external data.
    for (bool valid : previousRead) if (!valid) initialize = true;
    dut.set_initialize(initialize);
    dut.set_en(en);
    dut.set_write(write);
    dut.set_addr(address);
    dut.set_din(data);
    dut.set_gate(gate);
    std::array<uint8_t, 4> read{};
    for (unsigned i = 0; i < 4; ++i) read[i] = memory[i][previousAddress[i]];
    dut.step();
    const std::array<uint8_t, 4> actual{dut.get_plain(), dut.get_old_data(),
                                        dut.get_one(), dut.get_gated()};
    for (unsigned i = 0; i < 4; ++i) {
      if (previousRead[i] && actual[i] != read[i]) {
        std::fprintf(stderr, "cycle %u memory %u: got %u, expected %u\n",
                     cycle, i, actual[i], read[i]);
        std::exit(1);
      }
      const bool enabledClock = i != 3 || gate;
      const unsigned a = i == 2 ? 0 : address;
      if (enabledClock && en && write)
        memory[i][a] = initialize ? data : uint8_t(read[i] + 1);
      if (enabledClock) previousAddress[i] = a;
      previousRead[i] = enabledClock && en && !write;
    }
    ++cycle;
  };
  // Define all memory contents through port writes, never implicit init values.
  for (unsigned a = 0; a < 4; ++a) tick(true, true, true, a, 0x20 + a, true);
  tick(false, true, false, 2, 0, true);
  for (unsigned i = 0; i < 32; ++i) tick(true, true, true, 2, 0x72, true);
  tick(false, true, false, 2, 0, true);
  tick(false, true, false, 2, 0, true);
  uint32_t random = 0x7e57abcd;
  for (unsigned i = 0; i < 2000; ++i) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    tick(random & 1, (random >> 1) & 1, (random >> 2) & 1,
         (random >> 3) & 3, (random >> 8) & 255, (random >> 16) & 1);
  }
  // Drain responses, then read back every address (including constant inputs).
  for (unsigned a = 0; a < 4; ++a) {
    tick(false, true, false, a, 0, true);
    tick(false, true, false, a, 0, true);
  }
  std::puts("rdwr feedback, old, depth-one and gated clocks: PASS");
}
