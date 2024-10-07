#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <csignal>
#include <format>

#if defined(GSIM)
#include <SimTop.h>
MOD_NAME* mod;
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include HEADER
REF_NAME* ref;
#endif

#if defined(GSIM_DIFF)
#include <top_ref.h>
REF_NAME* ref;
#endif

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;
bool dut_end = false;

void load_program(char* filename){

  memset(&program, 0, sizeof(program));
  if(!filename){
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
  while(n --){
    ref->clock = 0;
    ref->eval();
    ref->clock = 1;
    ref->eval();
  }
}
void ref_reset() {
  ref->reset = 1;
  ref_cycle(1);

  ref->reset = 0;
  ref_cycle(1);
}
#endif
#ifdef GSIM
void mod_reset() {
  mod->set_reset(1);
  mod->step();

  mod->set_reset(0);
  mod->step();
}
#endif

int main(int argc, char** argv) {
  load_program(argv[1]);
#ifdef GSIM
  mod = new MOD_NAME();
  memcpy(&mod->mem$rdata_mem, program, program_sz);
  mod->set_difftest$$logCtrl$$begin(0);
  mod->set_difftest$$logCtrl$$end(0);
  mod->set_difftest$$uart$$in$$ch(-1);
  mod_reset();
#endif
#ifdef VERILATOR
  auto Context = std::make_unique<VerilatedContext>();
  ref = new REF_NAME(Context.get());
  memcpy(&ref->rootp->SimTop__DOT__mem__DOT__rdata_mem_ext__DOT__Memory, program, program_sz);
  ref->rootp->difftest_logCtrl_begin = ref->rootp->difftest_logCtrl_begin = 0;
  ref->rootp->difftest_uart_in_valid = -1;
  ref_reset();
#endif
  std::cout << "start testing.....\n";

  uint64_t cycles = 0;
  clock_t start = clock();
  while(!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);

#ifndef GSIM
    if (ref->rootp->difftest_uart_out_valid) {
      printf("%c", ref->rootp->difftest_uart_out_ch);
      fflush(stdout);
    }
#endif
#endif
    cycles ++;
    if(cycles % 10000000 == 0 && cycles <= 250000000) {
      clock_t dur = clock() - start;
      fprintf(stderr, "cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
#ifdef PERF
      size_t totalActives = 0;
      size_t validActives = 0;
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        totalActives += mod->activeTimes[i];
        validActives += mod->validActive[i];
      }
      printf("totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      fprintf(activeFp, "totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        fprintf(activeFp, "%ld: activeTimes %ld validActive %ld\n", i, mod->activeTimes[i], mod->validActive[i]);
      }
      if (cycles == 50000000) return 0;
#endif
      if (cycles == 250000000) return 0;
    }
#if defined(GSIM)
    mod->step();
    if (mod->get_difftest$$uart$$out$$valid()) {
      printf("%c", mod->get_difftest$$uart$$out$$ch());
      fflush(stdout);
    }
    // dut_end = mod->soc$nutcore$_dataBuffer_T_ctrl_isNutCoreTrap;
#endif
#if defined(GSIM) && defined(VERILATOR)
    bool error = false;
    auto report = [&](auto name, auto miss, auto right) {
      std::cerr << std::format("mismatching : {}\n\texpected: {:#x} unexpected: {:#x}\n", name, miss, right);
      error = true;
    };

    auto diff = [&](auto name, auto left, auto right) {
      if (left != right) report(name, left, right);
    };
#define VTOR_GET(X) ref->rootp->X
#define GSIM_GET(X) mod->X
#define DIFF(X, Y) diff(#X, VTOR_GET(Y), GSIM_GET(X))
    DIFF(soc$nutcore$frontend$ifu$pc, SimTop__DOT__soc__DOT__nutcore__DOT__frontend__DOT__ifu__DOT__pc);
    DIFF(soc$nutcore$frontend$ibf$state, SimTop__DOT__soc__DOT__nutcore__DOT__frontend__DOT__ibf__DOT__state);

    // io_i
    DIFF(soc$nutcore$io_imem_cache$s3$state, SimTop__DOT__soc__DOT__nutcore__DOT__io_imem_cache__DOT__s3__DOT__state);
    DIFF(soc$nutcore$io_imem_cache$s3$state2, SimTop__DOT__soc__DOT__nutcore__DOT__io_imem_cache__DOT__s3__DOT__state2);
    DIFF(soc$nutcore$io_imem_cache$valid, SimTop__DOT__soc__DOT__nutcore__DOT__io_imem_cache__DOT__valid);
    DIFF(soc$nutcore$io_imem_cache$valid_1, SimTop__DOT__soc__DOT__nutcore__DOT__io_imem_cache__DOT__valid_1);

    // mmioXbar
    DIFF(soc$nutcore$mmioXbar$state, SimTop__DOT__soc__DOT__nutcore__DOT__mmioXbar__DOT__state);
    DIFF(soc$nutcore$mmioXbar$inflightSrc, SimTop__DOT__soc__DOT__nutcore__DOT__mmioXbar__DOT__inflightSrc);

    // dmemXbar
    DIFF(soc$nutcore$dmemXbar$state, SimTop__DOT__soc__DOT__nutcore__DOT__dmemXbar__DOT__state);
    DIFF(soc$nutcore$dmemXbar$inflightSrc, SimTop__DOT__soc__DOT__nutcore__DOT__dmemXbar__DOT__inflightSrc);

    // Xbar
    DIFF(soc$xbar$state, SimTop__DOT__soc__DOT__xbar__DOT__state);

    // mem l2cache
    DIFF(soc$mem_l2cacheOut_cache$s3$state, SimTop__DOT__soc__DOT__mem_l2cacheOut_cache__DOT__s3__DOT__state);

    if (error) exit(EXIT_FAILURE);
#endif
#if (defined (VERILATOR))
  dut_end = ref->rootp->SimTop__DOT__soc__DOT__nutcore__DOT__dataBuffer_0_ctrl_isNutCoreTrap;
#endif
    if(dut_end) {
      clock_t dur = clock() - start;
    }
  }
  clock_t dur = clock() - start;
  printf("end cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
}