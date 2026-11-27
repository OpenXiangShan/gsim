#include <cstdio>

#include "SplitDynamicArray.h"

int main() {
  SSplitDynamicArray dut;
  for (unsigned flags = 0; flags < 8; flags ++) {
    for (unsigned row = 0; row < 4; row ++) {
      for (unsigned column = 0; column < 4; column ++) {
        for (unsigned data : {0u, 255u}) {
          dut.set_row(row);
          dut.set_column(column);
          dut.set_row_enable(flags & 1);
          dut.set_cell_enable((flags >> 1) & 1);
          dut.set_fixed_enable((flags >> 2) & 1);
          dut.set_data(data);
          dut.step();
          unsigned expected[4];
          for (unsigned index = 0; index < 4; index ++) {
            unsigned r = index / 2;
            unsigned c = index % 2;
            unsigned value = index ? expected[index - 1] : data;
            if ((flags & 1) && row == r) value = c ? 0x32 : 0x21;
            if ((flags & 2) && row == r && column == c) value = 0x55;
            if ((flags & 4) && r == 1 && column == c) value = 0x77;
            expected[index] = value;
          }
          const unsigned actual[] = {dut.get_element_0_0(), dut.get_element_0_1(),
                                     dut.get_element_1_0(), dut.get_element_1_1()};
          for (unsigned index = 0; index < 4; index ++) {
            if (actual[index] != expected[index]) {
              std::fprintf(stderr, "flags=%u row=%u column=%u element=%u: %u != %u\n",
                           flags, row, column, index, actual[index], expected[index]);
              return 1;
            }
          }
        }
      }
    }
  }
  std::puts("split dynamic array rows: PASS");
  return 0;
}
