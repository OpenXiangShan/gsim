#include <cstdint>
#include <cstdio>

#include "ArrayWidening.h"

int main() {
  SArrayWidening dut;
  for (unsigned a = 0; a < 8; ++a) {
    for (unsigned b = 0; b < 8; ++b) {
      const int sa = a < 4 ? a : static_cast<int>(a) - 8;
      const int sb = b < 4 ? b : static_cast<int>(b) - 8;
      dut.set_a(a);
      dut.set_b(b);
      dut.set_sa(static_cast<uint8_t>(sa));
      dut.set_sb(static_cast<uint8_t>(sb));
      for (unsigned index = 0; index < 2; ++index) {
        dut.set_index(index);
        dut.step();
        const unsigned u = index ? b : a;
        const int s = index ? sb : sa;
        const signed _BitInt(5) s5 = dut.get_s5();
        const signed _BitInt(9) s9 = dut.get_s9();
        const int64_t s64 = dut.get_s64();
        const signed _BitInt(128) s128 = dut.get_s128();
        const int64_t mixed = dut.get_mixed();
        if (dut.get_u5() != u || dut.get_u9() != u ||
            dut.get_u64() != u || dut.get_u128() != u ||
            s5 != s || s9 != s || s64 != s || s128 != s ||
            mixed != (index ? -2 : sa)) {
          std::fprintf(stderr, "array widening failed: a=%u b=%u index=%u\n", a, b, index);
          return 1;
        }
      }
    }
  }
  std::puts("array widening: PASS");
  return 0;
}
