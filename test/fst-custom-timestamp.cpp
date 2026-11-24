#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "FstCustomTimestamp.h"
#include "gsimFst.h"

namespace {

struct CapturedOutput {
  std::string out;
  std::string err;
};

template <typename Fn>
CapturedOutput captureStdStreams(Fn&& fn) {
  std::fflush(stdout);
  std::fflush(stderr);

  FILE* tmpOut = std::tmpfile();
  FILE* tmpErr = std::tmpfile();
  if (!tmpOut || !tmpErr) {
    std::fprintf(stderr, "tmpfile() failed\n");
    std::exit(1);
  }

  const int savedOut = dup(STDOUT_FILENO);
  const int savedErr = dup(STDERR_FILENO);
  if (savedOut < 0 || savedErr < 0) {
    std::fprintf(stderr, "dup() failed\n");
    std::exit(1);
  }

  if (dup2(fileno(tmpOut), STDOUT_FILENO) < 0 || dup2(fileno(tmpErr), STDERR_FILENO) < 0) {
    std::fprintf(stderr, "dup2() failed\n");
    std::exit(1);
  }

  fn();

  std::fflush(stdout);
  std::fflush(stderr);
  dup2(savedOut, STDOUT_FILENO);
  dup2(savedErr, STDERR_FILENO);
  close(savedOut);
  close(savedErr);

  auto readAll = [](FILE* fp) {
    std::rewind(fp);
    std::string data;
    char buf[256];
    while (size_t n = std::fread(buf, 1, sizeof(buf), fp)) {
      data.append(buf, n);
    }
    std::fclose(fp);
    return data;
  };

  return {readAll(tmpOut), readAll(tmpErr)};
}

struct ExpectedSample {
  uint64_t time;
  uint64_t out;
  uint64_t cycle;
};

struct IterState {
  std::vector<uint64_t> times;
  bool nonMonotonic = false;
};

void recordTimeCallback(void* userData, uint64_t time, fstHandle, const unsigned char*) {
  auto* state = static_cast<IterState*>(userData);
  if (!state->times.empty()) {
    if (time < state->times.back()) {
      state->nonMonotonic = true;
    }
    if (time == state->times.back()) {
      return;
    }
  }
  state->times.push_back(time);
}

bool checkU64AtTime(void* reader, uint64_t time, fstHandle handle, uint64_t expected, const char* sigName,
                    const char* path) {
  char buf[65] = {};
  const char* bits = fstReaderGetValueFromHandleAtTime(reader, time, handle, buf);
  if (!bits || std::strlen(bits) != 64 || std::strspn(bits, "01") != 64) {
    std::fprintf(stderr, "%s: invalid FST bits for '%s' at t=%" PRIu64 "\n", path, sigName, time);
    return false;
  }
  const uint64_t actual = std::strtoull(bits, nullptr, 2);
  if (actual != expected) {
    std::fprintf(stderr, "%s: mismatch for '%s' at t=%" PRIu64 ": expected 0x%" PRIx64 ", got 0x%" PRIx64 "\n", path,
                 sigName, time, expected, actual);
    return false;
  }
  return true;
}

bool verifyFstFile(const char* path, uint64_t expectedStart, uint64_t expectedEnd,
                   const std::vector<uint64_t>& expectedTimes, const std::vector<ExpectedSample>& queries) {
  void* reader = fstReaderOpen(path);
  if (!reader) {
    std::fprintf(stderr, "cannot open %s\n", path);
    return false;
  }

  fstHandle outHandle = 0;
  fstHandle cycleHandle = 0;
  while (auto* entry = fstReaderIterateHier(reader)) {
    if (entry->htyp != FST_HT_VAR) continue;
    if (std::strcmp(entry->u.var.name, "out") == 0 && entry->u.var.length == 64) {
      outHandle = entry->u.var.handle;
    } else if (std::strcmp(entry->u.var.name, "gsim_cycle") == 0 && entry->u.var.length == 64) {
      cycleHandle = entry->u.var.handle;
    }
  }
  if (!outHandle || !cycleHandle) {
    std::fprintf(stderr, "%s: missing expected FST handles (out=%u, gsim_cycle=%u)\n", path, outHandle, cycleHandle);
    fstReaderClose(reader);
    return false;
  }

  const uint64_t startTime = fstReaderGetStartTime(reader);
  const uint64_t endTime = fstReaderGetEndTime(reader);
  if (startTime != expectedStart || endTime != expectedEnd) {
    std::fprintf(stderr, "%s: time range mismatch: expected [%" PRIu64 ", %" PRIu64 "], got [%" PRIu64 ", %" PRIu64 "]\n",
                 path, expectedStart, expectedEnd, startTime, endTime);
    fstReaderClose(reader);
    return false;
  }

  IterState iterState;
  fstReaderSetFacProcessMaskAll(reader);
  fstReaderIterBlocks(reader, recordTimeCallback, &iterState, nullptr);
  if (iterState.nonMonotonic || iterState.times != expectedTimes) {
    std::fprintf(stderr, "%s: recorded timestamps mismatch (nonMonotonic=%d)\n", path, iterState.nonMonotonic);
    fstReaderClose(reader);
    return false;
  }

  for (const auto& q : queries) {
    if (!checkU64AtTime(reader, q.time, outHandle, q.out, "out", path) ||
        !checkU64AtTime(reader, q.time, cycleHandle, q.cycle, "gsim_cycle", path)) {
      fstReaderClose(reader);
      return false;
    }
  }

  fstReaderClose(reader);
  return true;
}

}  // namespace

int main() {
  constexpr const char* path1 = "fst-custom-timestamp-1.fst";
  constexpr const char* path2 = "fst-custom-timestamp-2.fst";

  {
    SFstCustomTimestamp dut;
    const SFstCustomTimestamp& cdut = dut;

    if (cdut.get_cycles() != 0 || cdut.get_cycles() != dut.cycles) {
      std::fprintf(stderr, "initial get_cycles() mismatch\n");
      return 1;
    }

    dut.setWaveformPath(path1);
    dut.enableWaveform();
    dut.set_reset(0);
    dut.set_didDump(1);
    dut.set_last_dump(0);

    // Phase 1: custom timestamps starting at t=0, plus non-monotonic calls.
    auto validPhase1 = captureStdStreams([&]() {
      dut.dump(0);  // t=0, out=0x0, cycle=0

      dut.set_last_dump(0x11);
      dut.step();
      dut.dump(10);  // t=10, out=0x11, cycle=1

      dut.set_last_dump(0x22);
      dut.step();
      dut.dump(25);  // t=25, out=0x33, cycle=2

      dut.set_last_dump(0x33);
      dut.step();
      dut.dump(40);  // t=40, out=0x66, cycle=3
    });
    if (!validPhase1.out.empty() || !validPhase1.err.empty()) {
      std::fprintf(stderr, "unexpected output during valid phase 1: out='%s' err='%s'\n", validPhase1.out.c_str(),
                   validPhase1.err.c_str());
      return 1;
    }
    if (cdut.get_cycles() != 3 || dut.get_out() != 0x66) {
      std::fprintf(stderr, "unexpected model state after phase 1\n");
      return 1;
    }

    // Mutate DUT state so an accidentally accepted dump would overwrite t=40 or inject t=20/30.
    dut.set_last_dump(0x100);
    dut.step();  // out=0x166, cycle=4

    auto invalidCalls = captureStdStreams([&]() {
      dut.dump(40);  // duplicate timestamp -> ignored
      dut.dump(20);  // backwards timestamp -> ignored, must not rewind fstLastDumpTime
      dut.dump(30);  // still < 40 -> must still be ignored
    });
    const std::string expectedWarnings =
        "WARNING: previous dump at t=40, requesting t=40, dump call ignored\n"
        "WARNING: previous dump at t=40, requesting t=20, dump call ignored\n"
        "WARNING: previous dump at t=40, requesting t=30, dump call ignored\n";
    if (!invalidCalls.out.empty()) {
      std::fprintf(stderr, "warning unexpectedly written to stdout: '%s'\n", invalidCalls.out.c_str());
      return 1;
    }
    if (invalidCalls.err != expectedWarnings) {
      std::fprintf(stderr, "stderr warning mismatch:\nexpected:\n%sactual:\n%s", expectedWarnings.c_str(),
                   invalidCalls.err.c_str());
      return 1;
    }

    // Subsequent valid timestamp > 40 must still succeed cleanly.
    auto resumeCall = captureStdStreams([&]() {
      dut.set_last_dump(0x200);
      dut.step();  // out=0x366, cycle=5
      dut.dump(50);
    });
    if (!resumeCall.out.empty() || !resumeCall.err.empty()) {
      std::fprintf(stderr, "unexpected output after resuming valid dump: out='%s' err='%s'\n", resumeCall.out.c_str(),
                   resumeCall.err.c_str());
      return 1;
    }

    // Phase 2: switching waveform path resets timestamp tracking; 0-arg dump() uses cycles;
    // disabled waveform ignores dump() without warning.
    dut.setWaveformPath(path2);
    auto phase2 = captureStdStreams([&]() {
      dut.dump(0);  // must succeed at t=0 on the new file (out=0x366, cycle=5)

      dut.set_last_dump(0x10);
      dut.step();  // out=0x376, cycle=6
      dut.dump();  // 0-arg overload -> dumps at t=cycles=6

      dut.disableWaveform();
      dut.set_last_dump(0x20);
      dut.step();  // out=0x396, cycle=7
      dut.dump(3);  // disabled -> must not warn or write

      dut.enableWaveform();
      dut.dump(12);  // t=12, out=0x396, cycle=7
    });
    if (!phase2.out.empty() || !phase2.err.empty()) {
      std::fprintf(stderr, "unexpected output during phase 2: out='%s' err='%s'\n", phase2.out.c_str(),
                   phase2.err.c_str());
      return 1;
    }
  }  // Close FST writers before reading back.

  if (!verifyFstFile(path1, 0, 50, {0, 10, 25, 40, 50},
                     {
                         {0, 0x0, 0},
                         {9, 0x0, 0},
                         {10, 0x11, 1},
                         {20, 0x11, 1},
                         {25, 0x33, 2},
                         {30, 0x33, 2},
                         {40, 0x66, 3},
                         {50, 0x366, 5},
                     })) {
    return 1;
  }

  if (!verifyFstFile(path2, 0, 12, {0, 6, 12},
                     {
                         {0, 0x366, 5},
                         {3, 0x366, 5},
                         {6, 0x376, 6},
                         {12, 0x396, 7},
                     })) {
    return 1;
  }

  std::puts("FST custom timestamp: PASS");
  return 0;
}
