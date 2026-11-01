#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "RdwrPipeline.h"

int main() {
  SRdwrPipeline dut;
  dut.set_clock(1);
  dut.set_reset(1);
  dut.set_initialize(0);
  dut.set_s0_valid(0);
  dut.set_s0_addr(0);
  dut.step();
  dut.step();
  dut.set_reset(0);
  // All contents are defined through real port writes.
  std::array<uint8_t, 4> memory{0x3f, 0x7f, 0xfe, 0x81};
  dut.set_initialize(1);
  for (unsigned a = 0; a < 4; ++a) {
    dut.set_init_addr(a);
    dut.set_init_data(memory[a]);
    dut.step();
  }
  dut.set_initialize(0);
  bool pending = false;
  unsigned pendingAddress = 0, cycle = 0;
  auto tick = [&](bool valid, unsigned address) {
    const uint8_t read = memory[pendingAddress];
    const bool fire = pending && !valid;
    dut.set_s0_valid(valid);
    dut.set_s0_addr(address);
    dut.step();
    if (dut.get_s1_valid() != pending || dut.get_s1_fire() != fire ||
        (pending && (dut.get_s1_rdata() != read ||
                     dut.get_s1_wdata() != uint8_t(read + 1)))) {
      std::fprintf(stderr, "cycle %u: got valid=%u fire=%u rdata=%u wdata=%u; expected valid=%u fire=%u rdata=%u\n",
                   cycle, dut.get_s1_valid(), dut.get_s1_fire(),
                   dut.get_s1_rdata(), dut.get_s1_wdata(), pending, fire, read);
      std::exit(1);
    }
    if (fire) memory[pendingAddress] = uint8_t(read + 1);
    pending = valid;
    if (valid) pendingAddress = address;
    ++cycle;
  };
  for (unsigned i = 0; i < 300; ++i) { tick(true, 2); tick(false, 2); }
  for (unsigned i = 0; i < 16; ++i) tick(true, i % 4);
  tick(false, 0);
  for (unsigned i = 0; i < 16; ++i) tick(false, i % 4);
  uint32_t random = 0x62345abc;
  for (unsigned i = 0; i < 4000; ++i) {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    tick(random & 1, (random >> 1) & 3);
  }
  tick(false, 0);
  tick(false, 0);
  for (unsigned a = 0; a < 4; ++a) { tick(true, a); tick(false, a); }
  std::printf("rdwr s0/s1 pipeline: PASS (%u cycles)\n", cycle);
}
