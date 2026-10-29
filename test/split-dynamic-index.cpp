#include <cstdio>

#include "SplitDynamicIndex.h"

int main() {
  SSplitDynamicIndex dut;
  for (unsigned data : {0u, 1u, 42u, 255u}) {
    for (unsigned index = 0; index < 4; index ++) {
      dut.set_data(data);
      dut.set_index(index);
      dut.step();
      unsigned expected = index < 3 ? 0x99 : data;
      if (dut.get_result() != expected) {
        std::fprintf(stderr, "data=%u index=%u: %u != %u\n",
                     data, index, dut.get_result(), expected);
        return 1;
      }
    }
  }
  std::puts("split dynamic vector index: PASS");
  return 0;
}
