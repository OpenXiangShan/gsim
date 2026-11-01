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
    if (dut.get_sum() != 16 * previous) {
      std::fprintf(stderr, "cycle %llu: got %llu, expected %llu\n",
                   (unsigned long long)cycle, (unsigned long long)dut.get_sum(),
                   (unsigned long long)(16 * previous));
      return 1;
    }
    previous = value;
  }
  std::puts("ok");
}
