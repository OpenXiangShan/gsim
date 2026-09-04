#include <cstdio>

#include "DshrOvershift.h"

int main() {
  SDshrOvershift dut;
  const unsigned _BitInt(128) ones = (unsigned _BitInt(128))-1;

  dut.set_unsigned_in(ones);
  dut.set_signed_in(-2);

  dut.set_shift(127);
  dut.step();
  if (dut.get_unsigned_out() != 1) return 1;
  if (dut.get_signed_out() != -1) return 2;

  dut.set_shift(128);
  dut.step();
  if (dut.get_unsigned_out() != 0) return 3;
  if (dut.get_signed_out() != -1) return 4;

  dut.set_shift(255);
  dut.step();
  if (dut.get_unsigned_out() != 0) return 5;
  if (dut.get_signed_out() != -1) return 6;

  dut.set_signed_in(2);
  dut.set_shift(128);
  dut.step();
  if (dut.get_signed_out() != 0) return 7;

  std::puts("dshr overshift: PASS");
  return 0;
}
