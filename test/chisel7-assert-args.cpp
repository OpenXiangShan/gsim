#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "Chisel7AssertArgs.h"

static int g_assert_hits = 0;

static void on_assert(const char*, long long) {
  g_assert_hits++;
}

static std::string run_and_capture_stderr(SChisel7AssertArgs& dut) {
  int pipefd[2];
  if (pipe(pipefd) != 0) std::abort();

  std::fflush(stderr);
  int saved_stderr = dup(STDERR_FILENO);
  if (saved_stderr < 0) std::abort();
  if (dup2(pipefd[1], STDERR_FILENO) < 0) std::abort();
  close(pipefd[1]);

  dut.step();
  std::fflush(stderr);

  if (dup2(saved_stderr, STDERR_FILENO) < 0) std::abort();
  close(saved_stderr);

  std::string out;
  char buf[512];
  ssize_t n;
  while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
    out.append(buf, static_cast<size_t>(n));
  }
  close(pipefd[0]);
  return out;
}

static void reset_enables(SChisel7AssertArgs& dut) {
  dut.set_enFmt(0);
  dut.set_enWhen(0);
  dut.set_enLegacy(0);
  dut.set_enNoFmt(0);
  dut.set_pred(1);
}

int main() {
  SChisel7AssertArgs dut;
  dut.register_assert(on_assert);

  dut.set_u8(42);
  dut.set_u4(0b1010);
  dut.set_u64(0x123456789abcdef0ULL);
  dut.set_ch('Z');

  // Case 0: all enables 0 -> no assert should fire (including NEVER_FIRE with neq(u8, u8))
  reset_enables(dut);
  dut.set_pred(0);
  std::string out0 = run_and_capture_stderr(dut);
  if (g_assert_hits != 0 || !out0.empty()) {
    std::fprintf(stderr, "Case 0 failed: hits=%d out=%s\n", g_assert_hits, out0.c_str());
    return 1;
  }

  // Case 1: Chisel 7 intrinsic assert with format args (%d, %x, %b, %c, %%, 64-bit, const)
  reset_enables(dut);
  dut.set_pred(0);
  dut.set_enFmt(1);
  g_assert_hits = 0;
  std::string out1 = run_and_capture_stderr(dut);
  const std::string expect1 = "FMT u8=42 u64=0x123456789abcdef0 bin=1010 ch=Z c64=7 esc=100%";
  if (g_assert_hits != 1 || out1.find(expect1) == std::string::npos) {
    std::fprintf(stderr, "Case 1 failed: hits=%d\n  expected substring: %s\n  got: %s\n",
                 g_assert_hits, expect1.c_str(), out1.c_str());
    return 2;
  }

  // Case 2: Chisel 7 intrinsic assert with args inside when + layerblock
  reset_enables(dut);
  dut.set_pred(0);
  dut.set_enWhen(1);
  g_assert_hits = 0;
  std::string out2 = run_and_capture_stderr(dut);
  const std::string expect2 = "WHEN u4=1010 u8=42";
  if (g_assert_hits != 1 || out2.find(expect2) == std::string::npos) {
    std::fprintf(stderr, "Case 2 failed: hits=%d\n  expected substring: %s\n  got: %s\n",
                 g_assert_hits, expect2.c_str(), out2.c_str());
    return 3;
  }

  // Case 3: Chisel 7 intrinsic assert without format parameter
  reset_enables(dut);
  dut.set_pred(0);
  dut.set_enNoFmt(1);
  g_assert_hits = 0;
  std::string out3 = run_and_capture_stderr(dut);
  if (g_assert_hits != 1 || out3.find("circt_chisel_ifelsefatal") == std::string::npos) {
    std::fprintf(stderr, "Case 3 failed: hits=%d got: %s\n", g_assert_hits, out3.c_str());
    return 4;
  }

  // Case 4: Legacy 3-child FIRRTL assert (top-level and inside when)
  reset_enables(dut);
  dut.set_pred(0);
  dut.set_enLegacy(1);
  g_assert_hits = 0;
  std::string out4 = run_and_capture_stderr(dut);
  if (g_assert_hits != 2 ||
      out4.find("LEGACY_TOP") == std::string::npos ||
      out4.find("LEGACY_WHEN") == std::string::npos) {
    std::fprintf(stderr, "Case 4 failed: hits=%d got: %s\n", g_assert_hits, out4.c_str());
    return 5;
  }

  // Case 5: pred=1 with all enables=1 -> no assert should fire
  dut.set_pred(1);
  dut.set_enFmt(1);
  dut.set_enWhen(1);
  dut.set_enLegacy(1);
  dut.set_enNoFmt(1);
  g_assert_hits = 0;
  std::string out5 = run_and_capture_stderr(dut);
  if (g_assert_hits != 0 || !out5.empty()) {
    std::fprintf(stderr, "Case 5 failed: hits=%d out=%s\n", g_assert_hits, out5.c_str());
    return 6;
  }

  std::puts("chisel7 assert args: PASS");
  return 0;
}
