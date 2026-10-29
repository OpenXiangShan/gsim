#include <cstdint>
#include <cstdio>

#include "TopVecAggregate.h"

int main() {
  STopVecAggregate dut;
  using U128 = unsigned _BitInt(128);
  const U128 mask65 = (U128(1) << 65) - 1;
  const unsigned constants[] = {0x12, 0x34, 0x56};
  for (unsigned round = 0; round < 512; ++round) {
    uint16_t expected_u[2][3], expected_s[2][3];
    U128 expected_wide[2];
    for (unsigned row = 0; row < 2; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
        const unsigned u = round * 37 + row * 113 + col * 71;
        const unsigned s = (round + row * 113 + col * 71) & 511;
        expected_u[row][col] = u & 511;
        expected_s[row][col] = static_cast<uint16_t>(s < 256 ? int(s) : int(s) - 512);
        // Include bits above the FIR width to exercise input normalization.
        dut.set_a$$u(row, col, u | 0xfe00);
        dut.set_a$$s(row, col, s | 0xfe00);
      }
      const U128 wide = (U128(1) << 100) | (U128(round & 1) << 64) | (round + row);
      expected_wide[row] = wide & mask65;
      dut.set_a$$wide(row, wide);
      // Port names also occur as generated parameter names.
      dut.set_i0(row, round + row);
      dut.set_val(row, round + row + 17);
    }
    for (unsigned col = 0; col < 3; ++col) dut.set_narrow(col, round + col * 17);
    dut.set_singleton(round);
    dut.set_select(round);
    dut.step();
    for (unsigned row = 0; row < 2; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
        if (dut.get_y$$u(row, col) != expected_u[row][col] ||
            dut.get_y$$s(row, col) != expected_s[row][col]) return 1;
      }
      if (dut.get_y$$wide(row) != expected_wide[row] ||
          dut.get_i1(row) != uint8_t(round + row) ||
          dut.get_val_out(row) != uint8_t(round + row + 17)) return 2;
    }
    for (unsigned col = 0; col < 3; ++col) {
      const unsigned varying = col == 1 ? uint8_t(round + 1) : (col == 0 ? 0x23 : 0x67);
      if (dut.get_low(col) != ((round + col * 17) & 7) ||
          dut.get_constant(col) != constants[col] ||
          dut.get_uniform(col) != 0x5a || dut.get_varying(col) != varying) return 3;
    }
    if (dut.get_singleton_out() != uint8_t(round) || dut.get_scalar() != (round & 1)) return 4;

    dut.step();
    dut.set_a$$u(1, 2, expected_u[1][2] ^ 511);
    dut.step();
    for (unsigned row = 0; row < 2; ++row) {
      for (unsigned col = 0; col < 3; ++col) {
        const unsigned expected = expected_u[row][col] ^ (row == 1 && col == 2 ? 511 : 0);
        if (dut.get_y$$u(row, col) != expected) return 5;
      }
    }
  }
#ifdef NDEBUG
  const auto last = dut.get_y$$u(1, 2);
  // The second dimension has three elements, although storage is padded to four.
  dut.set_a$$u(0, 3, 0xffff);
  dut.set_a$$u(2, 0, 0xffff);
  dut.set_a$$u(static_cast<std::size_t>(-1), 0, 0xffff);
  dut.set_narrow(3, 0xff);
  if (dut.get_y$$u(0, 3) != 0 || dut.get_y$$u(2, 0) != 0 ||
      dut.get_constant(3) != 0 || dut.get_uniform(3) != 0) return 6;
  dut.step();
  if (dut.get_y$$u(1, 2) != last) return 7;
#endif
  std::puts("top vector aggregate IO: PASS");
  return 0;
}
