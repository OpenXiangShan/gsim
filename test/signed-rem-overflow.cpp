#include <cstdint>
#include <cstdio>

#include "SignedRemOverflow.h"

template <int Width, typename Run>
bool check_width(Run run) {
  using U = unsigned _BitInt(Width);
  using S = signed _BitInt(Width);
  using Wide = signed _BitInt(Width + 1);
  const U sign = U(1) << (Width - 1);
  const U values[] = {0, 1, 2, 3, 7, sign - 1, sign, sign + 1, ~U(1), ~U(0)};
  for (U a : values) {
    for (U b : values) {
      if (b == 0) continue;
      const S expected = Wide(S(a)) % Wide(S(b));
      if (!run(S(a), S(b), expected)) {
        std::fprintf(stderr, "signed remainder failed at width %d\n", Width);
        return false;
      }
    }
  }
  return true;
}

int main() {
  SSignedRemOverflow dut;
  dut.set_a32(0); dut.set_b32(1);
  dut.set_a64(0); dut.set_b64(1);
  dut.set_a128(0); dut.set_b128(1);
  if (!check_width<32>([&](auto a, auto b, auto expected) {
        dut.set_a32(a); dut.set_b32(b); dut.step();
        return static_cast<int32_t>(dut.get_result32()) == expected;
      })) return 1;
  if (!check_width<64>([&](auto a, auto b, auto expected) {
        dut.set_a64(a); dut.set_b64(b); dut.step();
        const signed _BitInt(3) narrow = dut.get_narrow();
        return static_cast<int64_t>(dut.get_result64()) == expected &&
               dut.get_constant_divisor() == 0 &&
               narrow == static_cast<signed _BitInt(3)>(expected) &&
               dut.get_unsigned_result() == static_cast<uint64_t>(a) % static_cast<uint64_t>(b);
      })) return 1;
  if (!check_width<128>([&](auto a, auto b, auto expected) {
        dut.set_a128(a); dut.set_b128(b); dut.step();
        return static_cast<signed _BitInt(128)>(dut.get_result128()) == expected;
      })) return 1;
  std::puts("signed remainder overflow: PASS");
  return 0;
}
