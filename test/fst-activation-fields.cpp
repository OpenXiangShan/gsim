#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "FstActivationFields.h"
#include "gsimFst.h"

// Regression for FST field selection running before activation is finalized.
// The FIR feeds cross back into state and also uses shared/cross in its outputs.
// With one node per supernode (see .flags), these intermediates need persistent
// model storage. Before updateActivate(), isLocal() incorrectly treats them as
// temporaries, so the old FST inventory omits both even though the model works.
int main() {
  constexpr const char* path = "fst-activation-fields.fst";
  struct Sample {
    uint64_t time, shared, cross;
  };
  std::array<Sample, 4> samples;
  {
    SFstActivationFields dut;
    dut.set_reset(1);
    dut.set_a(0);
    dut.set_b(0);
    dut.step();
    dut.set_reset(0);
    dut.setWaveformPath(path);
    dut.enableWaveform();

    // Compute an independent reference from the reset state. Unsigned addition
    // wraps at 64 bits, matching the FIR's truncated add and feedback register.
    uint64_t state = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
      const uint64_t a = UINT64_C(0x123456789abcdef0) + i;
      const uint64_t b = UINT64_C(0xfedcba9876543210) - i;
      const uint64_t shared = a ^ state;
      const uint64_t cross = shared + b;
      dut.set_a(a);
      dut.set_b(b);
      dut.step();
      // First prove the intermediates exist in the model and compute correctly.
      // The unfixed generator passes this check; its failure is in the FST below.
      if (dut.shared != shared || dut.cross != cross || dut.get_first() != (shared | b) ||
          dut.get_second() != (cross & a)) {
        std::fprintf(stderr, "model mismatch at sample %zu\n", i);
        return 1;
      }
      // dump() records the evaluated state explicitly. Save its model timestamp
      // and reference values so the reader can check the exact same samples.
      dut.dump();
      samples[i] = {dut.cycles, shared, cross};
      state = cross;
    }
  }  // Close the writer before reading the actual FST.

  void* reader = fstReaderOpen(path);
  if (!reader) {
    std::fprintf(stderr, "cannot open %s\n", path);
    return 1;
  }
  // Read FST declarations first: each intermediate must have exactly one
  // 64-bit entry. The hierarchy supplies the handles used for value lookups;
  // opening the file or checking model outputs alone cannot detect missing fields.
  const std::array<const char*, 2> names = {"shared", "cross"};
  std::array<fstHandle, 2> handles = {};
  bool failed = false;
  while (auto* entry = fstReaderIterateHier(reader)) {
    if (entry->htyp != FST_HT_VAR) continue;
    for (size_t i = 0; i < names.size(); ++i) {
      if (std::strcmp(entry->u.var.name, names[i]) != 0) continue;
      if (handles[i] || entry->u.var.length != 64) {
        std::fprintf(stderr, "duplicate or wrong-width FST signal '%s'\n", names[i]);
        failed = true;
      }
      handles[i] = entry->u.var.handle;
    }
  }
  // This is where the unfixed generator fails: neither signal was declared,
  // despite both model members passing the arithmetic checks above.
  for (size_t i = 0; i < names.size(); ++i) {
    if (!handles[i]) {
      std::fprintf(stderr, "missing FST signal '%s' (model member exists)\n", names[i]);
      failed = true;
    }
  }
  if (failed) {
    fstReaderClose(reader);
    return 1;
  }

  // A declaration alone is insufficient: the writer must also emit the right
  // value at every explicit dump. Query the recorded timestamps and compare the
  // reader's binary strings against the independent reference, including wraparound.
  for (const auto& sample : samples) {
    const std::array<uint64_t, 2> expected = {sample.shared, sample.cross};
    for (size_t i = 0; i < names.size(); ++i) {
      char buffer[65] = {};
      const char* bits = fstReaderGetValueFromHandleAtTime(reader, sample.time, handles[i], buffer);
      if (!bits || std::strlen(bits) != 64 || std::strspn(bits, "01") != 64 ||
          std::strtoull(bits, nullptr, 2) != expected[i]) {
        std::fprintf(stderr, "FST value mismatch for '%s' at time %" PRIu64 "\n", names[i], sample.time);
        fstReaderClose(reader);
        return 1;
      }
    }
  }
  fstReaderClose(reader);
  std::puts("FST activation fields: PASS (2 signals, 4 samples)");
  return 0;
}
