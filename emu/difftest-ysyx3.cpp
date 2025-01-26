#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>
#include <csignal>

#if defined(GSIM)
#include <newtop.h>
MOD_NAME* mod;
#endif

#if defined(GSIM_DIFF)
#include <top_ref.h>
REF_NAME* ref;
#endif

#if defined(VERILATOR)
#include "verilated.h"
#include HEADER
REF_NAME* ref;
#endif

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;
bool dut_end = false;

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
  for (int i = 0; i < 10; i ++) {
    mod->step();
  }
  mod->set_reset(0);
}
#endif
#ifdef GSIM_DIFF
void ref_reset() {
  ref->set_reset(1);
  for (int i = 0; i < 10; i ++)
    ref->step();
  ref->set_reset(0);
}
#endif

#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)

bool checkSig(bool display, REF_NAME* ref, MOD_NAME* mod);
bool checkSignals(bool display) {
  return checkSig(display, ref, mod);
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
#ifdef GSIM_DIFF
  ref = new REF_NAME();
  memcpy(&ref->mem$ram, program, program_sz);
  ref_reset();
  ref->step();
#endif
#if (defined(VERILATOR) || defined(GSIM_DIFF)) && defined(GSIM)
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed init\nALL diffs: mode -- ref\n";
      checkSignals(false);
      return 0;
    }
#endif
  std::cout << "start testing.....\n";
  // printf("size = %lx %lx\n", sizeof(*ref->rootp),
  // (uintptr_t)&(ref->rootp->newtop__DOT__cpu__DOT__icache__DOT__Ram_bw__DOT__ram__DOT__ram) - (uintptr_t)(ref->rootp));  
  std::signal(SIGINT, [](int){
    dut_end = true;
  });
  std::signal(SIGTERM, [](int){
    dut_end = true;
  });
  uint64_t cycles = 0;
#ifdef PERF
  FILE* activeFp = fopen(ACTIVE_FILE, "w");
#endif
  size_t nodeNum = 0;
  for (size_t i = 0; i < sizeof(mod->nodeNum) / sizeof(mod->nodeNum[0]); i ++) {
    nodeNum += mod->nodeNum[i];
  }
  clock_t start = clock();
  while(!dut_end) {
#ifdef VERILATOR
    ref_cycle(1);
#endif
#ifdef GSIM_DIFF
    ref->step();
#endif
    cycles ++;
    // if (cpu)
#if (!defined(GSIM) && defined(VERILATOR)) || (defined(GSIM) && !defined(VERILATOR))
    if(cycles % 1000000 == 0 && cycles <= 600000000) {
      clock_t dur = clock() - start;
      fprintf(stderr, "cycles %d (%d ms, %d per sec) \n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
#ifdef PERF
      size_t totalActives = 0;
      size_t validActives = 0;
      size_t nodeActives = 0;
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        totalActives += mod->activeTimes[i];
        validActives += mod->validActive[i];
        nodeActives += mod->nodeNum[i] * mod->activeTimes[i];
      }
      printf("nodeNum %ld totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld nodeActive %ld\n",
          nodeNum, totalActives, totalActives / cycles, validActives, validActives / cycles, nodeActives);
      fprintf(activeFp, "totalActives %ld activePerCycle %ld totalValid %ld validPerCycle %ld\n",
          totalActives, totalActives / cycles, validActives, validActives / cycles);
      for (size_t i = 1; i < sizeof(mod->activeTimes) / sizeof(mod->activeTimes[0]); i ++) {
        fprintf(activeFp, "%ld %s : activeTimes %ld validActive %ld\n", i, mod->activeName[i].c_str(), mod->activeTimes[i], mod->validActive[i]);
      }
      if (cycles == 50000000) return 0;
#endif
      if (cycles == 600000000) return 0;
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
      if(mod->cpu$regs$regs[0] == 0){
#else
      if(ref->rootp->newtop__DOT__cpu__DOT__regs__DOT__regs_0 == 0) {
#endif
          printf("\33[1;32mCPU HIT GOOD TRAP after %d cycles (%d ms, %d per sec)\033[0m\n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
      }else{
          printf("\33[1;31mCPU HIT BAD TRAP after %d cycles\033[0m\n", cycles);
      }
    }
  }
}