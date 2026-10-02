#include "MtStateTasks.h"
#include <cstdint>
#include <cstdio>

int main() {
  SMtStateTasks dut;
  uint8_t q0 = 0, q1 = 0, qa = 0, qb = 0, qr = 0, ql = 0;
  uint8_t next0 = 0, next1 = 0, nexta = 0, nextb = 0, nextr = 0, nextl = 0;
  uint64_t signature = 1469598103934665603ULL;
  for (unsigned cycle = 0; cycle < 1024; ++cycle) {
    bool reset = cycle < 3 || cycle % 79 == 0;
    bool asyncReset = cycle < 2 || cycle % 47 == 0;
    bool enable = cycle % 5 != 0;
    bool request = cycle % 13 < 2;
    uint8_t a = cycle * 17 + 3, b = cycle * 29 + 5;
    // A register-driven reset uses the pre-commit snapshot of qr.
    qb = qr ? 0x78 : nextb;
    q0 = reset ? 0x12 : next0;
    q1 = reset ? 0x34 : next1;
    qr = reset ? 0 : nextr;
    qa = asyncReset ? 0x56 : nexta;
    ql = (request != enable) ? 0x9a : nextl;
    dut.set_clock(cycle & 1);
    dut.set_reset(reset);
    dut.set_async_reset(asyncReset);
    dut.set_reset_request(request);
    dut.set_enable(enable);
    dut.set_a(a);
    dut.set_b(b);
    dut.step();
    if (dut.get_value0() != q0 || dut.get_value1() != q1 ||
        dut.get_value_async() != qa || dut.get_value_reg_reset() != qb ||
        dut.get_reset_value() != qr || dut.get_value_late_reset() != ql ||
        dut.get_mix() != (q0 ^ q1 ^ qa ^ qb)) {
      std::fprintf(stderr, "state mismatch at cycle %u: q0=%u/%u q1=%u/%u "
                   "qa=%u/%u qb=%u/%u qr=%u/%u\n", cycle,
                   dut.get_value0(), q0, dut.get_value1(), q1, dut.get_value_async(), qa,
                   dut.get_value_reg_reset(), qb, dut.get_reset_value(), qr);
      return 1;
    }
    signature = (signature ^ dut.get_mix()) * 1099511628211ULL;
    next0 = enable ? static_cast<uint8_t>(q1 + a) : q0;
    next1 = q0 ^ b;
    nexta = qa + q0;
    nextb = qb + b;
    nextr = request;
    nextl = ql + a;
  }
  std::printf("%016llx\n", static_cast<unsigned long long>(signature));
}
