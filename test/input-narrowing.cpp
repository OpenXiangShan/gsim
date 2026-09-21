#include <cstdint>
#include <cstdio>

#include "InputNarrowing.h"

int main() {
  SInputNarrowing dut;
  using U128 = unsigned _BitInt(128);
  const uint64_t mask33 = (uint64_t(1) << 33) - 1;
  const U128 mask65 = (U128(1) << 65) - 1;
  // Alternate high bits even when the effective low bits do not change.
  const uint64_t values[] = {31, 7, 255, 0, 8, 1, 511, 3, 4, ~uint64_t(0)};
  for (uint64_t value : values) {
    const uint64_t wide = value | (uint64_t(1) << 40);
    const U128 wider = (U128(1) << 100) | U128(value);
    dut.set_data9(value & 511);
    dut.set_data64(wide);
    dut.set_data128(wider);
    dut.set_signed9(value & 511);
    dut.step();
    const unsigned low = value & 7;
    const int signed_low = low < 4 ? low : static_cast<int>(low) - 8;
    const signed _BitInt(3) actual_signed = dut.get_signed_narrow();
    if (dut.get_narrow3() != low || dut.get_equal3() != (low == 7) ||
        dut.get_narrow33() != (wide & mask33) ||
        dut.get_equal33() != ((wide & mask33) == 7) ||
        dut.get_narrow65() != (wider & mask65) ||
        dut.get_equal65() != ((wider & mask65) == 7) ||
        actual_signed != signed_low || dut.get_negative() != (signed_low < 0) ||
        static_cast<int64_t>(dut.get_signed_wide()) != signed_low) {
      std::fprintf(stderr, "input narrowing failed for 0x%llx\n", static_cast<unsigned long long>(value));
      return 1;
    }
  }
  std::puts("input narrowing: PASS");
  return 0;
}
