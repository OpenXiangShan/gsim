#include "FlowArraySplit.h"

#include <cstdio>

int main() {
  SFlowArraySplit dut;
  dut.set_idx(1);
  dut.set_count(1);
  dut.set_bankIn(7);
  dut.set_b00(1);
  dut.set_b01(2);
  dut.set_b10(3);
  dut.set_b11(4);
  dut.step();
  dut.step();
  if (dut.get_glued() != 2 || dut.get_lane0() != 3 || dut.get_lane1() != 4 || dut.get_spill() != 2 ||
      dut.get_ob0() != 7 || dut.get_ob1() != 7 || dut.get_og0() != 2 || dut.get_og1() != 4 ||
      dut.get_os0() != 0 || dut.get_os1() != 7 || dut.get_obulk0() != 5 || dut.get_obulk1() != 6 ||
      dut.get_pack0() != 3 || dut.get_pack1() != 4 || dut.get_hit0() != 3 || dut.get_hit1() != 4) {
    printf("idx1 glued=%u lane=%u %u spill=%u bank=%u %u grid=%u %u slot=%u %u bulk=%u %u pack=%u %u hit=%u %u\n",
           dut.get_glued(), dut.get_lane0(), dut.get_lane1(), dut.get_spill(),
           dut.get_ob0(), dut.get_ob1(), dut.get_og0(), dut.get_og1(), dut.get_os0(), dut.get_os1(),
           dut.get_obulk0(), dut.get_obulk1(), dut.get_pack0(), dut.get_pack1(), dut.get_hit0(), dut.get_hit1());
    return 1;
  }
  dut.set_idx(0);
  dut.set_count(0);
  dut.step();
  dut.step();
  if (dut.get_glued() != 1 || dut.get_lane0() != 1 || dut.get_lane1() != 2 || dut.get_spill() != 1 ||
      dut.get_og0() != 1 || dut.get_og1() != 3 || dut.get_os0() != 7 || dut.get_os1() != 7 ||
      dut.get_obulk0() != 5 || dut.get_obulk1() != 6 || dut.get_pack0() != 1 || dut.get_pack1() != 2 ||
      dut.get_hit0() != 7 || dut.get_hit1() != 10) {
    printf("idx0 glued=%u lane=%u %u spill=%u grid=%u %u slot=%u %u bulk=%u %u pack=%u %u hit=%u %u\n",
           dut.get_glued(), dut.get_lane0(), dut.get_lane1(), dut.get_spill(),
           dut.get_og0(), dut.get_og1(), dut.get_os0(), dut.get_os1(), dut.get_obulk0(), dut.get_obulk1(),
           dut.get_pack0(), dut.get_pack1(), dut.get_hit0(), dut.get_hit1());
    return 1;
  }
  puts("ok");
  return 0;
}
