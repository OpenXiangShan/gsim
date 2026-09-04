#include <cstdio>

#include "SignedNodeShr.h"

int main() {
  SSignedNodeShr dut;
  dut.step();

  const signed _BitInt(7) shifted = dut.get_shifted();
  if (shifted != -3) return 1;

  std::puts("signed node shr: PASS");
  return 0;
}
