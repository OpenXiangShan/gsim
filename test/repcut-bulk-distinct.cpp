#include "RcBulkCommit.h"

#include <cstdint>
#include <cstdio>

int main() {
  SRcBulkCommit dut;
  uint64_t previous = 0;
  for (uint64_t cycle = 0; cycle < 128; ++cycle) {
    const uint64_t value = (cycle / 7 * 17) ^ ((cycle / 7) << 32);
    dut.set_inc(value);
    dut.step();
    if (dut.get_sum() != 16 * previous ||
        dut.get_tap1() != (previous ^ 0x55) ||
        dut.get_tap2() != (previous ^ 0x33) ||
        dut.get_tap3() != (previous ^ 0x77)) {
      std::fprintf(stderr, "cycle %llu: incorrect output\n", (unsigned long long)cycle);
      return 1;
    }
    previous = value;
  }
  std::puts("ok");
}
