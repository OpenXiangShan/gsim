#include <cstdio>

#include "SignedConstantSlice.h"

int main() {
  SSignedConstantSlice dut;
  dut.step();

  const signed _BitInt(7) shifted = dut.get_shifted();
  if (shifted != -3) return 1;
  if (dut.get_selected() != 5) return 2;
  const signed _BitInt(12) padded = dut.get_padded();
  if (padded != -5) return 3;
  if (dut.get_padded_high() != 0xf) return 4;
  const signed _BitInt(4) padded_shifted = dut.get_padded_shifted();
  if (padded_shifted != -1) return 5;
  const signed _BitInt(8) pad_identity = dut.get_pad_identity();
  if (pad_identity != -5) return 6;

  std::puts("signed constant slice: PASS");
  return 0;
}
