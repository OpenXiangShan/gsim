#include <cstdio>

#include "AsyncResetHold.h"

int main() {
  SAsyncResetHold dut;
  struct Stage {
    unsigned reset, en, data, zero, nonzero;
  };
  const Stage stages[] = {
      {1, 0, 0x00, 0x00, 0xa5},
      {0, 1, 0x2a, 0x2a, 0x2a},
      {0, 0, 0x00, 0x2a, 0x2a},
      {1, 1, 0x55, 0x00, 0xa5},
      {1, 1, 0xc3, 0x00, 0xa5},
      {0, 0, 0x00, 0x00, 0xa5},
      {0, 1, 0x55, 0x55, 0x55},
      {1, 0, 0x00, 0x00, 0xa5},
      {0, 0, 0x00, 0x00, 0xa5},
      {0, 1, 0x00, 0x00, 0x00},
      {1, 1, 0x55, 0x00, 0xa5},
      {0, 1, 0x55, 0x55, 0x55},
  };
  bool passed = true;
  for (unsigned stage = 0; stage < sizeof(stages) / sizeof(stages[0]); ++stage) {
    const Stage& s = stages[stage];
    dut.set_reset(s.reset);
    dut.set_en(s.en);
    dut.set_data(s.data);
    for (unsigned cycle = 0; cycle < 8; ++cycle) {
      dut.set_clock(1);
      dut.step();
      dut.set_clock(0);
      // Check stable stages after the register update has propagated.
      if (cycle >= 2 && (dut.get_zero() != s.zero || dut.get_nonzero() != s.nonzero)) {
        std::fprintf(stderr, "async reset stage %u cycle %u: got %02x/%02x, expected %02x/%02x\n",
            stage, cycle, unsigned(dut.get_zero()), unsigned(dut.get_nonzero()), s.zero, s.nonzero);
        passed = false;
      }
    }
  }
  if (!passed) return 1;
  std::puts("async reset: PASS");
  return 0;
}
