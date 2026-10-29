#include <cstdint>
#include <cstdio>
#include "TopVecIO.h"

int main() {
  STopVecIO dut;
  for (unsigned value = 0; value < 256; ++value) {
    dut.set_a(0, value);
    dut.set_a(1, value ^ 0xa5);
    dut.step();
    if (dut.get_y(0) != value || dut.get_y(1) != (value ^ 0xa5)) return 1;
    // Change one element after the model has gone idle; its dependents must run.
    dut.step();
    dut.set_a(1, value);
    dut.step();
    if (dut.get_y(0) != value || dut.get_y(1) != value) return 2;
  }
#ifdef NDEBUG
  // Bounds use FIR dimensions, not the padded C++ storage dimensions.
  dut.set_a(2, 0xff);
  dut.set_a(static_cast<std::size_t>(-1), 0xff);
  if (dut.get_y(2) != 0 || dut.get_y(static_cast<std::size_t>(-1)) != 0) return 3;
  dut.step();
  if (dut.get_y(0) != 255 || dut.get_y(1) != 255) return 4;
#endif
  std::puts("top vector IO: PASS");
}
