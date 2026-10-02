#include "MtSparseRegisterAsyncReset.h"

#include <cstdint>
#include <cstdio>

int main() {
  SMtSparseRegisterAsyncReset dut;
  uint64_t signature = 1469598103934665603ULL;
  for (unsigned cycle = 0; cycle < 1024; ++cycle) {
    dut.set_clock(cycle & 1u);
    dut.set_reset(cycle < 3 || cycle == 509);
    dut.set_io_index((cycle * 3u + 1u) & 3u);
    dut.set_io_write_a((cycle % 5u) == 0);
    dut.set_io_write_b((cycle % 7u) == 0);
    dut.set_io_data_a((cycle * 13u + 9u) & 0xffu);
    dut.set_io_data_b((cycle * 29u + 3u) & 0xffu);
    dut.step();
    signature = (signature ^ dut.get_io_value()) * 1099511628211ULL;
  }
  std::printf("%016llx\n", static_cast<unsigned long long>(signature));
  return 0;
}
