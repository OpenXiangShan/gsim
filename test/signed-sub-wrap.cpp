#include <cstdint>
#include <cstdio>

#include "SignedSubWrap.h"

template <int Width, typename Run>
bool check_width(Run run) {
  using U = unsigned _BitInt(Width);
  using S = signed _BitInt(Width);
  const U sign = U(1) << (Width - 1);
  const U values[] = {0, 1, 2, sign - 1, sign, sign + 1, ~U(0)};
  for (U a : values) {
    for (U b : values) {
      const S expected = S(U(a - b));
      if (!run(S(a), S(b), expected, expected < S(a))) {
        std::fprintf(stderr, "signed subtraction failed at width %d\n", Width);
        return false;
      }
    }
  }
  return true;
}

int main() {
  SSignedSubWrap dut;
  dut.set_a9(0); dut.set_b9(0);
  dut.set_a32(0); dut.set_b32(0);
  dut.set_a64(0); dut.set_b64(0);
  dut.set_a128(0); dut.set_b128(0);
  if (!check_width<9>([&](auto a, auto b, auto expected, bool less) {
        dut.set_a9(static_cast<uint16_t>(a));
        dut.set_b9(static_cast<uint16_t>(b));
        dut.step();
        return static_cast<signed _BitInt(9)>(dut.get_result9()) == expected &&
               dut.get_less9() == less;
      })) return 1;
  if (!check_width<32>([&](auto a, auto b, auto expected, bool less) {
        dut.set_a32(a); dut.set_b32(b); dut.step();
        return static_cast<int32_t>(dut.get_result32()) == expected && dut.get_less32() == less;
      })) return 1;
  if (!check_width<64>([&](auto a, auto b, auto expected, bool less) {
        dut.set_a64(a); dut.set_b64(b); dut.step();
        const int64_t decrement = static_cast<uint64_t>(a) - uint64_t(1);
        const signed _BitInt(65) wide = static_cast<signed _BitInt(65)>(a) - b;
        return static_cast<int64_t>(dut.get_result64()) == expected &&
               dut.get_less64() == less && dut.get_decrement_less() == (decrement < a) &&
               static_cast<signed _BitInt(65)>(dut.get_wide_difference()) == wide &&
               static_cast<int64_t>(dut.get_shifted_difference()) == (wide >> 1);
      })) return 1;
  if (!check_width<128>([&](auto a, auto b, auto expected, bool less) {
        dut.set_a128(a); dut.set_b128(b); dut.step();
        return static_cast<signed _BitInt(128)>(dut.get_result128()) == expected &&
               dut.get_less128() == less;
      })) return 1;
  std::puts("signed subtraction wrap: PASS");
  return 0;
}
