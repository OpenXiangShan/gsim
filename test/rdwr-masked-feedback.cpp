#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "RdwrMaskedFeedback.h"

int main() {
  SRdwrMaskedFeedback dut;
  dut.set_clock(1);
  std::array<std::array<uint8_t, 2>, 4> memory{};
  unsigned previousAddress = 0, cycle = 0;
  bool previousRead = false;
  auto tick = [&](bool initialize, bool en, bool write, unsigned address,
                  unsigned data0, unsigned data1, unsigned mask) {
    // Avoid propagating undefined read results back into memory.
    initialize = initialize || !previousRead;
    dut.set_initialize(initialize);
    dut.set_en(en);
    dut.set_write(write);
    dut.set_addr(address);
    dut.set_din0(data0);
    dut.set_din1(data1);
    dut.set_mask0(mask & 1);
    dut.set_mask1((mask >> 1) & 1);
    const auto read = memory[previousAddress];
    dut.step();
    if (previousRead && (dut.get_rdata0() != read[0] || dut.get_rdata1() != read[1])) {
      std::fprintf(stderr, "cycle %u: got (%u,%u), expected (%u,%u)\n", cycle,
                   dut.get_rdata0(), dut.get_rdata1(), read[0], read[1]);
      std::exit(1);
    }
    if (en && write) {
      if (mask & 1) memory[address][0] = initialize ? data0 : uint8_t(read[0] + 1);
      if (mask & 2) memory[address][1] = initialize ? data1 : uint8_t(read[1] ^ 0xa5);
    }
    previousAddress = address;
    previousRead = en && !write;
    ++cycle;
  };
  for (unsigned a = 0; a < 4; ++a) tick(true, true, true, a, 0x30 + a, 0x90 + a, 3);
  tick(false, true, false, 1, 0, 0, 0);
  for (unsigned i = 0; i < 32; ++i) tick(true, true, true, 1, 0x72, 0, 1);
  for (unsigned a = 0; a < 4; ++a) {
    tick(false, true, false, a, 0, 0, 0);
    tick(false, true, false, a, 0, 0, 0);
  }
  uint32_t random = 0x123abcde;
  for (unsigned i = 0; i < 2000; ++i) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    tick(random & 1, (random >> 1) & 1, (random >> 2) & 1,
         (random >> 3) & 3, (random >> 8) & 255, (random >> 16) & 255,
         (random >> 24) & 3);
  }
  for (unsigned a = 0; a < 4; ++a) {
    tick(false, true, false, a, 0, 0, 0);
    tick(false, true, false, a, 0, 0, 0);
  }
  std::puts("rdwr masked feedback: PASS");
}
