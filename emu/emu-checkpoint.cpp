#include <cstdlib>
#include <iostream>
#include <bits/types/FILE.h>
#include <time.h>
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <set>
#include <csignal>
#include <chrono>
#include <sys/mman.h>
#include <sys/stat.h>

#include "support/compress.h"

#define DUT_MEMORY memory$ram$rdata_mem$mem
#define REF_MEMORY SimTop__DOT__memory__DOT__ram__DOT__rdata_mem__DOT__mem_ext__DOT__Memory

#define MAX_PROGRAM_SIZE (8 * 1024 * 1024 * 1024UL)
#define MAX_INSTS 40000000
size_t warmupInsts = 20000000;

#if defined(GSIM_DIFF)
#include <top_ref.h>
REF_NAME* ref;
#endif

#if defined(GSIM)
#include DUT_HEADER
DUT_NAME* dut;
// unused blackbox
void imsic_csr_top(uint8_t _0, uint8_t _1, uint16_t _2, uint8_t _3, uint8_t _4, uint16_t _5, uint8_t _6, uint8_t _7, uint8_t _8, uint8_t _9, uint8_t _10, uint8_t _11, uint64_t _12, uint8_t& _13, uint64_t& _14, uint8_t& _15, uint8_t& _16, uint32_t& _17, uint32_t& _18, uint32_t& _19) {
  _13 = 0;
  _15 = 0;
  _16 = 0; // o_irq
  _17 = 0;
  _18 = 0;
  _19 = 0;
}

// unused blackbox
void SimJTAG(uint8_t _0, uint8_t& _1, uint8_t& _2, uint8_t& _3, uint8_t _4, uint8_t _5, uint8_t _6, uint8_t _7, uint32_t& _8) {
  _1 = 1; // TRSTn
  _2 = 0; // TMS
  _3 = 0; // TDI
  _8 = 0;
}

// unused blackbox
void FlashHelper(uint8_t en, uint32_t addr, uint64_t &data) {
  data = 0;
}

// unused blackbox
void SDCardHelper(uint8_t setAddr, uint32_t addr, uint8_t ren, uint32_t& data) {
  data = 0;
}

void dut_init(DUT_NAME *dut) {
  dut->set_difftest$$perfCtrl$$clean(0);
  dut->set_difftest$$perfCtrl$$dump(0);
  dut->set_difftest$$logCtrl$$begin(0);
  dut->set_difftest$$logCtrl$$end(0);
  dut->set_difftest$$logCtrl$$level(0);
  dut->set_difftest$$uart$$in$$ch(-1);
}


void dut_hook(DUT_NAME *dut) {
  if (dut->get_difftest$$uart$$out$$valid()) {
    printf("%c", dut->get_difftest$$uart$$out$$ch());
    fflush(stdout);
  }
}
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include REF_HEADER
REF_NAME* ref;
void ref_init(REF_NAME *ref) {
  ref->difftest_perfCtrl_clean = ref->difftest_perfCtrl_dump = 0;
  ref->difftest_uart_in_ch = -1;
  ref->difftest_uart_in_valid = 0;
}

void ref_hook(REF_NAME *ref) {
  if (ref->difftest_uart_out_valid) {
    printf("%c", ref->difftest_uart_out_ch);
    fflush(stdout);
  }
}
#endif

static size_t program_sz = 0;
static int program_fd = 0;
static void *program = NULL;
static bool dut_end = false;


template <typename T>
std::vector<size_t> sort_indexes(const std::vector<T>& v) {
  // initialize original index locations
  std::vector<size_t> idx(v.size());
  iota(idx.begin(), idx.end(), 0);

  // when v contains elements of equal values
  stable_sort(idx.begin(), idx.end(), [&v](size_t i1, size_t i2) { return v[i1] < v[i2]; });

  return idx;
}

void readFromBin(const char* filename) {
  assert(filename != NULL);
  program_fd = open(filename, O_RDONLY);
  assert(program_fd != -1);

  struct stat s;
  int ret = fstat(program_fd, &s);
  assert(ret == 0);
  program_sz = s.st_size;

  program = mmap(NULL, program_sz, PROT_READ, MAP_PRIVATE, program_fd, 0);
  assert(program != (void *)-1); 
}

void load_program(char* filename) {
  assert(filename != NULL);
  printf("The image is %s\n", filename);
  if (isGzFile(filename)) {
    program = (uint64_t *)mmap(NULL, MAX_PROGRAM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    printf("Gzip file detected and loading image from extracted gz file\n");
    program_sz = readFromGz(program, filename, MAX_PROGRAM_SIZE, LOAD_RAM);
  } else if (isZstdFile(filename)) {
    program = (uint64_t *)mmap(NULL, MAX_PROGRAM_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    printf("Zstd file detected and loading image from extracted zstd file\n");
    program_sz = readFromZstd(program, filename, MAX_PROGRAM_SIZE, LOAD_RAM);
  } else {
    printf("Bin file detected and loading image from binary file\n");
    readFromBin(filename);
  }

  printf("load program size: 0x%lx\n", program_sz);
  assert(program_sz > 0 && program_sz <= MAX_PROGRAM_SIZE);
  return;
}

static void close_program() {
  munmap(program, program_sz);
  close(program_fd);
}

void overwrite_ram(const char* filename) {
  std::fstream fs(filename, std::ios::binary | std::ios::in);
  if (!fs.is_open()) {
    printf("Failed to open: %s\n", filename);
    exit(EXIT_FAILURE);
  }

  if (!fs.read((char*)program, 0xe00)) {
    printf("Failed to read: %s\n", filename);
    exit(EXIT_FAILURE);
  }

  fs.close();
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
  load_program(argv[2]);
  overwrite_ram(argv[1]);
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
  close_program();
  std::cout << "start testing.....\n";
  std::signal(SIGINT, [](int){ dut_end = true; });
  std::signal(SIGTERM, [](int){ dut_end = true; });
  uint64_t cycles = 0;
  #ifdef PERF
    FILE* activeFp = fopen(ACTIVE_FILE, "w");
  #endif
  std::size_t instrCnt = 0;
  auto start = std::chrono::system_clock::now();
  while (!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
    ref_hook(ref);
    instrCnt += ref->rootp->SimTop__DOT__l_soc__DOT__core_with_l2__DOT__core__DOT__backend__DOT__inner__DOT__ctrlBlock__DOT__io_robio_csr_perfinfo_retiredInstr_REG;
    if (instrCnt >= warmupInsts) {
      ref->difftest_perfCtrl_clean = 1;
      ref->difftest_perfCtrl_dump = 1;
      std::cout << "cycleCnt = " << cycles << std::endl;
      std::cout << "instrCnt = " << instrCnt << std::endl;
    }
#endif
#if defined(GSIM)
    dut_cycle(1);
    dut_hook(dut);
  #if not defined(GSIM_DIFF)
    instrCnt += dut->l_soc$core_with_l2$core$backend$inner$ctrlBlock$io_robio_csr_perfinfo_retiredInstr_REG;
  #endif
    if (instrCnt >= warmupInsts) {
      printf("Warmup finished. The performance counters will be dumped and then reset.\n");
      dut->set_difftest$$perfCtrl$$clean(true);
      dut->set_difftest$$perfCtrl$$dump(true);
      std::cout << "cycleCnt = " << cycles << std::endl;
      std::cout << "instrCnt = " << instrCnt << std::endl;
    }
#endif
#ifdef GSIM_DIFF
    ref_cycle(1);
#endif
  if (instrCnt >= warmupInsts) warmupInsts = -1;
#ifdef GSIM_DIFF
    ref->step();
#endif
    cycles++;
    #if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if(isDiff) {
      printf("all Sigs:\n -----------------\n");
      checkSignals(true);
      printf("ALL diffs: dut -- ref\n");
      printf("Failed after %ld cycles\n", cycles);
      checkSignals(false);
      return -1;
    }
#endif
    dut_end = instrCnt >= MAX_INSTS;
    if ((cycles % 100000) == 0 || dut_end) {
      auto dur = std::chrono::system_clock::now() - start;
      auto msec = std::chrono::duration_cast<std::chrono::milliseconds>(dur);
      fprintf(stderr, "cycles %ld (%ld ms, %ld per sec) \n",
          cycles, msec.count(), cycles * 1000 / msec.count());
    }

  }

  auto now = std::chrono::system_clock::now();
  auto dur = now - start;

  std::cout << "Host time spent: " << std::chrono::duration_cast<std::chrono::milliseconds>(dur).count() << "ms" << std::endl;
  return 0;
}
