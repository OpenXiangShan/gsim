#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>

#if defined(GSIM)
#include <top.h>
MOD_NAME* mod;
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include HEADER
REF_NAME* ref;
#endif

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;

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
void ref_reset(){
    ref->reset = 1;
    for(int i = 0; i < 10; i++){
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
#ifdef GSIM_DIFF
void ref_reset() {
  ref->set_reset(1);
  ref->step();
  ref->set_reset(0);
}
#endif

#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)

bool checkSignals(bool display) {
  #include "../obj/checkSig.h"
}
#endif

int main(int argc, char** argv) {
  load_program(argv[1]);
#ifdef GSIM
  mod = new MOD_NAME();
  memcpy(&mod->mem$rdata_mem$mem, program, program_sz);
  mod->set_io_logCtrl_log_begin(0);
  mod->set_io_logCtrl_log_end(0);
  mod->set_io_uart_in_ch(-1);
  mod_reset();
  mod->step();
#endif
#ifdef VERILATOR
  ref = new REF_NAME();
  memcpy(&ref->rootp->SimTop__DOT__mem__DOT__rdata_mem__DOT__mem, program, program_sz);
  ref->io_logCtrl_log_begin = ref->io_logCtrl_log_begin = 0;
  ref->io_uart_in_ch = -1;
  ref_reset();
#endif
#ifdef GSIM_DIFF
  ref = new REF_NAME();
  memcpy(&ref->mem$rdata_mem$mem, program, program_sz);
  ref_reset();
  ref->step();
#endif
  std::cout << "start testing.....\n";
  bool dut_end = false;
  uint64_t cycles = 0;
  clock_t start = clock();
  while(!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
  #ifndef GSIM
    if (ref->io_uart_out_valid) {
      printf("%c", ref->io_uart_out_ch);
      fflush(stdout);
    }
  #endif
#endif
#ifdef GSIM_DIFF
    ref->step();
#endif
    cycles ++;
#if (!defined(GSIM) && defined(VERILATOR)) || (defined(GSIM) && !defined(VERILATOR))
    if(cycles % 1000000 == 0 && cycles < 600000000) {
      clock_t dur = clock() - start;
      // printf("cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
      // return 0;
      // if (cycles == 571000000) return 0;
    }
#if 0
    if (cycles % 10000000 == 0 && cycles < 600000000) {
      std::ofstream out("data/active/activeTimes" + std::to_string(cycles / 10000000) + ".txt");
      std::vector<uint64_t> activeTimes(mod->allActiveTimes);
      std::vector<uint64_t> sorted = sort_indexes(activeTimes);
      out << "posActives " << mod->posActivate << " " << mod->posActivate / cycles << " actives " << mod->activeNum / cycles << std::endl;
      out << "funcTime " << mod->funcTime << " activeTime " << mod->activeTime << " regsTime " << mod->regsTime << " memoryTime " << mod->memoryTime << std::endl;
      for (int i = sorted.size()-1; i >= 0; i --) {
        if (mod->allNames[sorted[i]].length() == 0) continue;
        out << mod->allNames[sorted[i]] << " " << mod->nodeNum[sorted[i]] << " " << (double)activeTimes[sorted[i]] / cycles << " " << activeTimes[sorted[i]] << " " \
            << mod->posActives[sorted[i]] << " "  << (double)mod->posActives[sorted[i]] / activeTimes[sorted[i]] << std::endl;
      }
    }
#endif
#endif
#if defined(GSIM)
    mod->step();
    if (mod->io_uart_out_valid) {
      printf("%c", mod->io_uart_out_ch);
      fflush(stdout);
    }
    // dut_end = (mod->cpu$writeback$valid_r == 1) && (mod->cpu$writeback$inst_r == 0x6b);
#endif
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
    if(dut_end) {
      clock_t dur = clock() - start;
#if defined(GSIM)
      // if(mod->cpu$regs$regs[0] == 0){
#else
      // if(ref->rootp->newtop__DOT__cpu__DOT__regs__DOT__regs_0 == 0) {
#endif
      //     printf("\33[1;32mCPU HIT GOOD TRAP after %d cycles (%d ms, %d per sec)\033[0m\n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
      // }else{
      //     printf("\33[1;31mCPU HIT BAD TRAP after %d cycles\033[0m\n", cycles);
      // }
    }
  }
}