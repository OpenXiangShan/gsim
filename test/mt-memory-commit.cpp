#include "MtMemoryCommit.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

int main() {
  setenv("GSIM_MT_EXECUTOR", "dense", 1);
  SMtMemoryCommit dut;
  uint64_t signature = 1469598103934665603ULL;
  for (unsigned cycle = 0; cycle < 10000; ++cycle) {
    dut.set_clock(cycle & 1);
    dut.set_reset(cycle < 4);
    dut.set_wen((cycle % 3) != 0);
    dut.set_waddr(static_cast<uint8_t>((cycle * 3 + 1) & 3));
    dut.set_raddr(static_cast<uint8_t>((cycle * 5 + 2) & 3));
    dut.set_wdata(static_cast<uint8_t>(cycle * 17 + 9));
    dut.set_wmask(static_cast<uint8_t>((cycle % 3) + 1));
    dut.set_rw_en((cycle % 5) != 0);
    dut.set_rw_wmode((cycle % 4) == 1);
    dut.set_rw_addr(static_cast<uint8_t>((cycle * 7 + 3) & 3));
    dut.set_rw_data(static_cast<uint8_t>(cycle * 29 + 11));
    dut.step();
    signature = (signature ^ dut.get_comb_out()) * 1099511628211ULL;
    signature = (signature ^ dut.get_sync_out_0()) * 1099511628211ULL;
    signature = (signature ^ dut.get_sync_out_1()) * 1099511628211ULL;
    signature = (signature ^ dut.get_rw_out()) * 1099511628211ULL;
    signature = (signature ^ dut.get_block_out_0()) * 1099511628211ULL;
    signature = (signature ^ dut.get_block_out_1()) * 1099511628211ULL;
    signature = (signature ^ dut.get_multi_out()) * 1099511628211ULL;
  }
  std::printf("%016llx\n", static_cast<unsigned long long>(signature));
}
