#include <cstdio>

#include "CatEqConstant.h"

int main() {
  SCatEqConstant dut;

  dut.set_lhs(0xa);
  dut.set_rhs(0xb);
  dut.step();
  if (dut.get_out() != 1) return 1;
  if (dut.get_inverted() != 0) return 2;

  dut.set_lhs(0x5);
  dut.step();
  if (dut.get_out() != 0) return 3;

  dut.set_lhs(0xa);
  dut.set_rhs(0xa);
  dut.step();
  if (dut.get_out() != 0) return 4;

  std::puts("cat equality constant: PASS");
  return 0;
}
