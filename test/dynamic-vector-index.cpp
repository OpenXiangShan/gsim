#include <cstdio>

#include "DynamicVectorIndex.h"

int main() {
  SDynamicVectorIndex dut;

  dut.set_write_enable(0);
  dut.set_read_index(1);
  dut.step();
  if (dut.get_selected() != 0x22) return 1;
  if (dut.get_copied_0() != 0x50) return 2;
  if (dut.get_copied_1() != 0x51) return 3;

  dut.set_read_index(15);
  dut.step();
  if (dut.get_selected() != 0) return 4;
  if (dut.get_copied_0() != 0x40) return 5;
  if (dut.get_copied_1() != 0x41) return 6;

  dut.set_write_enable(1);
  dut.set_read_index(1);
  dut.set_write_index(1);
  dut.step();
  if (dut.get_selected() != 0x99) return 7;
  if (dut.get_element_1() != 0x99) return 8;
  if (dut.get_matrix_1_0() != 0x61) return 9;
  if (dut.get_matrix_1_1() != 0x62) return 10;

  dut.set_write_index(15);
  dut.step();
  if (dut.get_element_0() != 0x11) return 11;
  if (dut.get_element_1() != 0x22) return 12;
  if (dut.get_element_2() != 0x33) return 13;
  if (dut.get_matrix_0_0() != 0x40) return 14;
  if (dut.get_matrix_0_1() != 0x41) return 15;
  if (dut.get_matrix_1_0() != 0x50) return 16;
  if (dut.get_matrix_1_1() != 0x51) return 17;

  std::puts("dynamic vector index: PASS");
  return 0;
}
