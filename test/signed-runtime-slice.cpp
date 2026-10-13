#include "SignedRuntimeSlice.h"
#include <cstdio>

int main() {
  SSignedRuntimeSlice dut;
  for (unsigned value = 0; value < 32; ++value) {
    dut.set_value(value);
    dut.step();
    if (dut.get_result() != value) {
      std::fprintf(stderr,
                   "signed full-width bits failed: value=%u expected=%u actual=%u\n",
                   value, value, static_cast<unsigned>(dut.get_result()));
      return 1;
    }
  }
  std::puts("signed full-width bits: PASS");
}
