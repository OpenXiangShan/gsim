#include <cstdint>
#include <cstdio>

#include "ArrayNarrowing.h"

int main() {
  SArrayNarrowing dut;
  bool passed = true;
  auto check = [&](const char* signal, int actual, int expected) {
    if (actual != expected) {
      std::fprintf(stderr, "%s: got %d, expected %d\n", signal, actual, expected);
      passed = false;
    }
  };

  dut.set_column(0);
  dut.set_unsigned_0(0);
  dut.set_unsigned_1(0);
  dut.set_unsigned_2(0);
  dut.set_signed_0(0);
  dut.set_signed_1(0);
  dut.set_signed_2(0);
  dut.set_reset(1);
  dut.set_initialize(0);
  dut.set_index(0);
  dut.step();
  dut.set_reset(0);
  dut.set_initialize(1);
  dut.step();
  dut.set_initialize(0);

  // A UInt<3> row is assigned to a UInt<2> register row each cycle. The
  // dynamic read keeps the two-dimensional register represented as an array.
  unsigned expected_dependency = 1;
  unsigned expected_signed_dependency = 5; // SInt<3>(-3)
  for (unsigned cycle = 0; cycle < 10; ++cycle) {
    dut.set_index(cycle & 1);
    dut.step();
    check("dependency", dut.get_selected(), expected_dependency);
    expected_dependency = (expected_dependency << 1) & 3;
    // Widen after each signed row assignment so lost sign normalization is
    // observable even when the low three bits alone happen to be correct.
    const signed _BitInt(9) signed_dependency = dut.get_signed_dependency();
    const int signed_expected = expected_signed_dependency < 4
        ? expected_signed_dependency : static_cast<int>(expected_signed_dependency) - 8;
    check("signed dependency", signed_dependency, signed_expected);
    const signed _BitInt(3) signed_raw = dut.get_signed_dependency_raw();
    check("signed dependency raw", signed_raw, signed_expected);
    expected_signed_dependency = (expected_signed_dependency << 1) & 7;
  }

  // Exercise whole-array connects across the uint8_t/uint16_t storage
  // boundary, including sign extension after signed narrowing.
  constexpr unsigned unsigned_values[] = {
      0, 1, 3, 7, 8, 15, 127, 128, 255, 256, 257, 511};
  constexpr int signed_values[] = {
      -256, -255, -129, -128, -9, -8, -5, -4, -1, 0, 3, 4, 7, 8, 127, 255};
  for (unsigned test = 0; test < 16; ++test) {
    unsigned u[3];
    int s[3];
    for (unsigned column = 0; column < 3; ++column) {
      u[column] = unsigned_values[(test + column) % 12];
      s[column] = signed_values[(test + column) % 16];
    }
    dut.set_unsigned_0(u[0]);
    dut.set_unsigned_1(u[1]);
    dut.set_unsigned_2(u[2]);
    dut.set_signed_0(static_cast<uint16_t>(s[0]));
    dut.set_signed_1(static_cast<uint16_t>(s[1]));
    dut.set_signed_2(static_cast<uint16_t>(s[2]));
    for (unsigned column = 0; column < 3; ++column) {
      dut.set_column(column);
      dut.step();
      check("unsigned source", dut.get_unsigned_source(), u[column]);
      check("unsigned narrow", dut.get_unsigned_narrow(), u[column] & 7);
      check("unsigned wide", dut.get_unsigned_wide(), u[column] & 7);
      const signed _BitInt(9) signed_source = dut.get_signed_source();
      const signed _BitInt(3) signed_narrow = dut.get_signed_narrow();
      const signed _BitInt(9) signed_wide = dut.get_signed_wide();
      const int low_bits = static_cast<unsigned>(s[column]) & 7;
      const int expected_signed = low_bits < 4 ? low_bits : low_bits - 8;
      check("signed source", signed_source, s[column]);
      check("signed narrow", signed_narrow, expected_signed);
      check("signed wide", signed_wide, expected_signed);
    }
  }

  if (!passed) return 1;
  std::puts("array narrowing: PASS");
  return 0;
}
