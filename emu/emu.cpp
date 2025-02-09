#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <csignal>
#include <chrono>

#if defined(__DUT_ysyx3__)
#define DUT_MEMORY mem$ram
#define REF_MEMORY newtop__DOT__mem__DOT__ram_ext__DOT__Memory
#define CYCLE_MAX_PERF 50000000
#define CYCLE_MAX_SIM  600000000
#define CYCLE_MSG_STEP 10000000
#elif defined(__DUT_NutShell__)
#define DUT_MEMORY mem$rdata_mem$mem
#define REF_MEMORY SimTop__DOT__mem__DOT__rdata_mem__DOT__mem_ext__DOT__Memory
#define CYCLE_MAX_PERF 50000000
#define CYCLE_MAX_SIM  250000000
#define CYCLE_MSG_STEP 10000000
#elif defined(__DUT_rocket__)
#define DUT_MEMORY mem$srams$mem
#define REF_MEMORY TestHarness__DOT__mem__DOT__srams__DOT__mem_ext__DOT__Memory
#define CYCLE_MAX_PERF 30000000
#define CYCLE_MAX_SIM  70000000
#define CYCLE_MSG_STEP 10000000
#elif defined(__DUT_boom__) || defined(__DUT_small_boom__)
#define DUT_MEMORY mem$srams$mem
#define REF_MEMORY TestHarness__DOT__mem__DOT__srams__DOT__mem_ext__DOT__Memory
#define CYCLE_MAX_PERF 50000000
#define CYCLE_MAX_SIM  100000000
#define CYCLE_MSG_STEP 10000000
#elif defined(__DUT_xiangshan__) || defined(__DUT_xiangshan_default__)
#define DUT_MEMORY memory$ram$rdata_mem$mem
#define REF_MEMORY SimTop__DOT__memory__DOT__ram__DOT__rdata_mem__DOT__mem
#define CYCLE_MAX_PERF 500000
#ifdef __DUT_xiangshan__
#define CYCLE_MAX_SIM  6400000
#else
#define CYCLE_MAX_SIM  3100000
#endif
#define CYCLE_MSG_STEP 100000
#else
#error Unsupport DUT = DUT_NAME
#endif

#if defined(GSIM)
#include DUT_HEADER
static DUT_NAME* dut;

void dut_init(DUT_NAME *dut) {
#if defined(__DUT_NutShell__)
  dut->set_difftest$$logCtrl$$begin(0);
  dut->set_difftest$$logCtrl$$end(0);
  dut->set_difftest$$uart$$in$$ch(-1);
#elif defined(__DUT_xiangshan__) || defined(__DUT_xiangshan_default__)
  dut->set_difftest$$perfCtrl$$clean(0);
  dut->set_difftest$$perfCtrl$$dump(0);
  dut->set_difftest$$logCtrl$$begin(0);
  dut->set_difftest$$logCtrl$$end(0);
  dut->set_difftest$$logCtrl$$level(0);
  dut->set_difftest$$uart$$in$$ch(-1);
#endif
}

void dut_hook(DUT_NAME *dut) {
#if defined(__DUT_NutShell__)
  if (dut->get_difftest$$uart$$out$$valid()) {
    printf("%c", dut->get_difftest$$uart$$out$$ch());
    fflush(stdout);
  }
#elif defined(__DUT_xiangshan__) || defined(__DUT_xiangshan_default__)
  if (dut->get_difftest$$uart$$out$$valid()) {
    printf("%c", dut->get_difftest$$uart$$out$$ch());
    fflush(stdout);
  }
#endif
}
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include REF_HEADER
static REF_NAME* ref;

void ref_init(REF_NAME *ref) {
#if defined(__DUT_NutShell__)
  ref->rootp->difftest_logCtrl_begin = ref->rootp->difftest_logCtrl_begin = 0;
  ref->rootp->difftest_uart_in_valid = -1;
#elif defined(__DUT_xiangshan__) || defined(__DUT_xiangshan_default__)
  ref->difftest_perfCtrl_clean = ref->difftest_perfCtrl_dump = 0;
  ref->difftest_uart_in_ch = -1;
  ref->difftest_uart_in_valid = 0;
#endif
}

void ref_hook(REF_NAME *ref) {
#if defined(__DUT_NutShell__)
  if (ref->rootp->difftest_uart_out_valid) {
    printf("%c", ref->rootp->difftest_uart_out_ch);
    fflush(stdout);
  }
#elif defined(__DUT_xiangshan__) || defined(__DUT_xiangshan_default__)
  if (ref->difftest_uart_out_valid) {
    printf("%c", ref->difftest_uart_out_ch);
    fflush(stdout);
  }
#endif
}
#endif

#if defined(GSIM_DIFF)
#error not support now
#include <top_ref.h>
REF_NAME* ref;
#endif

#define MAX_PROGRAM_SIZE 0x8000000
static uint8_t program[MAX_PROGRAM_SIZE];
static int program_sz = 0;
static bool dut_end = false;

template <typename T>
std::vector<size_t> sort_indexes(const std::vector<T> &v) {

  // initialize original index locations
  std::vector<size_t> idx(v.size());
  iota(idx.begin(), idx.end(), 0);

  // when v contains elements of equal values
  stable_sort(idx.begin(), idx.end(),
       [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

  return idx;
}

void load_program(char* filename) {
  memset(&program, 0, sizeof(program));
  if (!filename) {
    printf("No input program\n");
    return;
  }

  FILE* fp = fopen(filename, "rb");
  assert(fp);

  fseek(fp, 0, SEEK_END);
  program_sz = ftell(fp);
  assert(program_sz < MAX_PROGRAM_SIZE);

  fseek(fp, 0, SEEK_SET);
  int ret = fread(program, program_sz, 1, fp);
  assert(ret == 1);
  printf("load program size: 0x%x\n", program_sz);
  return;
}

#ifdef GSIM
void dut_cycle(int n) { while (n --) dut->step(); }
void dut_reset() { dut->set_reset(1); dut_cycle(10); dut->set_reset(0); }
#endif
#ifdef VERILATOR
void ref_cycle(int n) {
  while (n --) {
    ref->clock = 0; ref->eval();
    ref->clock = 1; ref->eval();
  }
}
void ref_reset() { ref->reset = 1; ref_cycle(10); ref->reset = 0; }
#endif
#ifdef GSIM_DIFF
void ref_cycle(int n) { while (n --) ref->step(); }
void ref_reset() { ref->set_reset(1); ref_cycle(10); ref->set_reset(0); }
#endif

#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
bool checkSig(bool display, REF_NAME* ref, DUT_NAME* dut);
bool checkSignals(bool display) {
  return checkSig(display, ref, dut);
}
#endif

int main(int argc, char** argv) {
  load_program(argv[1]);
#ifdef GSIM
  dut = new DUT_NAME();
  memcpy(&dut->DUT_MEMORY, program, program_sz);
  dut_init(dut);
  dut_reset();
  dut_cycle(1);
#endif
#ifdef VERILATOR
  ref = new REF_NAME();
  memcpy(&ref->rootp->REF_MEMORY, program, program_sz);
  ref_init(ref);
  ref_reset();
#endif
#ifdef GSIM_DIFF
  ref = new REF_NAME();
  memcpy(&ref->DUT_MEMORY, program, program_sz);
  ref_reset();
#endif
  std::cout << "start testing.....\n";
  std::signal(SIGINT, [](int){ dut_end = true; });
  std::signal(SIGTERM, [](int){ dut_end = true; });
  uint64_t cycles = 0;
#ifdef PERF
  FILE* activeFp = fopen(ACTIVE_FILE, "w");
#endif
  auto start = std::chrono::system_clock::now();
  while (!dut_end) {
#if defined(GSIM)
    dut_cycle(1);
    dut_hook(dut);
#endif
#ifdef VERILATOR
    ref_cycle(1);
    ref_hook(ref);
#endif
#ifdef GSIM_DIFF
    ref_cycle(1);
#endif
    cycles ++;
#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed after " << cycles << " cycles\nALL diffs: mode -- ref\n";
      checkSignals(false);
      return 0;
    }
#endif
    if (cycles % CYCLE_MSG_STEP == 0 && cycles <= CYCLE_MAX_SIM) {
      auto dur = std::chrono::system_clock::now() - start;
      auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
      fprintf(stderr, "cycles %ld (%ld ms, %ld per sec) \n", cycles, msec.count(), cycles * 1000 / msec.count());
#ifdef PERF
      size_t totalActives = 0;
      size_t validActives = 0;
      size_t nodeActives = 0;
      for (size_t i = 0; i < sizeof(dut->activeTimes) / sizeof(dut->activeTimes[0]); i ++) {
        totalActives += dut->activeTimes[i];
        validActives += dut->validActive[i];
        nodeActives += dut->nodeNum[i] * dut->activeTimes[i];
      }
      printf("nodeNum %ld totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld nodeActive %ld\n",
          nodeNum, totalActives, totalActives / cycles, validActives, validActives / cycles, nodeActives);
      fprintf(activeFp, "totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      for (size_t i = 1; i < sizeof(dut->activeTimes) / sizeof(dut->activeTimes[0]); i ++) {
        fprintf(activeFp, "%ld: activeTimes %ld validActive %ld\n", i, dut->activeTimes[i], dut->validActive[i]);
      }

      std::ofstream out("data/active/activeTimes" + std::to_string(cycles / 10000000) + ".txt");
      std::vector<uint64_t> activeTimes(dut->allActiveTimes);
      std::vector<uint64_t> sorted = sort_indexes(activeTimes);
      out << "posActives " << dut->posActivate << " " << dut->posActivate / cycles << " actives " << dut->activeNum / cycles << std::endl;
      out << "funcTime " << dut->funcTime << " activeTime " << dut->activeTime << " regsTime " << dut->regsTime << " memoryTime " << dut->memoryTime << std::endl;
      for (int i = sorted.size()-1; i >= 0; i --) {
        if (dut->allNames[sorted[i]].length() == 0) continue;
        out << dut->allNames[sorted[i]] << " " << dut->nodeNum[sorted[i]] << " " << (double)activeTimes[sorted[i]] / cycles << " " << activeTimes[sorted[i]] << " " \
            << dut->posActives[sorted[i]] << " "  << (double)dut->posActives[sorted[i]] / activeTimes[sorted[i]] << std::endl;
      }

      if (cycles == CYCLE_MAX_PERF) return 0;
#endif
      if (cycles == CYCLE_MAX_SIM) return 0;
    }
  }
}
