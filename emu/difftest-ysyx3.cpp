#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>

#if defined(GSIM)
#include <top.h>
MOD_NAME* mod;
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include HEADER
REF_NAME* ref;
extern "C" void update_reg(int id, long long val) {}
extern "C" void update_indi(svBit cpu_is_mmio, svBit cpu_valid, int rcsr_id) {}
extern "C" void update_pc(long long pc, int inst) {}
extern "C" void update_csr(int id, long long val) {}
extern "C" void update_priv(int priv) {}
extern "C" void update_excep(svBit intr, long long cause, long long pc) {}
extern "C" void sdcard_read(int offset, long long* rdata) {}
extern "C" void sdcard_write(int offset, long long wdata) {}
#endif

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;

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

#ifdef VERILATOR
void ref_cycle(int n) {
  while (n--) {
    ref->clock = 0;
    ref->eval();
    ref->clock = 1;
    ref->eval();
  }
}
void ref_reset() {
  ref->reset = 1;
  for (int i = 0; i < 10; i++) {
    ref_cycle(1);
  }
  ref->reset = 0;
}
#endif
#ifdef GSIM
void mod_reset() {
  mod->set_reset(1);
  mod->step();
  mod->set_reset(0);
}
#endif
#if defined(VERILATOR) && defined(GSIM)
bool checkSignals(bool display) {
#include "../obj/checkSig.h"
}
#endif

int main(int argc, char** argv) {
  load_program(argv[1]);
#ifdef GSIM
  mod = new MOD_NAME();
  memcpy(&mod->mem$ram, program, program_sz);
  mod_reset();
  mod->step();
#endif
#ifdef VERILATOR
  ref = new REF_NAME();
  memcpy(&ref->rootp->newtop__DOT__mem__DOT__ram, program, program_sz);
  ref_reset();
#endif
#if defined(VERILATOR) && defined(GSIM)
  // if(checkSignals(false)) {
  //   std::cout << "Diff after reset!\n";
  //   checkSignals(true);
  //   return 0;
  // }
#endif
  std::cout << "start testing.....\n";
  bool dut_end = false;
  int cycles = 0;
  clock_t start = clock();
  while (!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
#endif
    cycles++;
#if (!defined(GSIM) && defined(VERILATOR)) || (defined(GSIM) && !defined(VERILATOR))
    if (cycles % 1000000 == 0) {
      clock_t dur = clock() - start;
      printf("cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC,
             cycles * CLOCKS_PER_SEC / dur);
    }
#endif
#if defined(GSIM)
    mod->step();
    dut_end = (mod->cpu$writeback$valid_r == 1) && (mod->cpu$writeback$inst_r == 0x6b);
#endif
#if defined(VERILATOR) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if (isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed after " << cycles << " cycles\nALL diffs: mode -- ref\n";
      checkSignals(false);
      return 0;
    }
#endif
    if (dut_end) {
      clock_t dur = clock() - start;
#if defined(GSIM)
      if (mod->cpu$regs$regs_0 == 0) {
#else
      if (ref->rootp->newtop__DOT__cpu__DOT__regs__DOT__regs_0 == 0) {
#endif
        printf("\33[1;32mCPU HIT GOOD TRAP after %d cycles (%d ms, %d per sec)\033[0m\n", cycles,
               dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
      } else {
        printf("\33[1;31mCPU HIT BAD TRAP after %d cycles\033[0m\n", cycles);
      }
    }
  }
}
