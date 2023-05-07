#include <iostream>
#include <top.h>
#include <time.h>
#include "verilated.h"
#include HEADER

MOD_NAME* mod;
REF_NAME* ref;

#define MAX_PROGRAM_SIZE 0x8000000
uint8_t program[MAX_PROGRAM_SIZE];
int program_sz = 0;

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

#ifdef DIFFTEST
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

void mod_reset() {
  mpz_t val;
  mpz_init(val);
  mpz_set_ui(val, 1);
  mod->set_reset(val);
  mod->step();
  mpz_set_ui(val, 0);
  mod->set_reset(val);
}

bool checkSignals(bool display) {
  #include "../obj/checkSig.h"
}

int main(int argc, char** argv) {
  mod = new MOD_NAME();
  ref = new REF_NAME();
  load_program(argv[1]);
  memcpy(&mod->mem$ram, program, program_sz);
#ifdef DIFFTEST
  memcpy(&ref->rootp->top__DOT__mem__DOT__ram, program, program_sz);
  ref_reset();
#endif
  mod_reset();
  mod->step();
#ifdef DIFFTEST
  if(checkSignals(false)) {
    std::cout << "Diff after reset!\n";
    return 0;
  }
#endif
  std::cout << "start testing.....\n";
  bool dut_end = false;
  int cycles = 0;
  clock_t start = clock();
  while(!dut_end) {
    mod->step();
    cycles ++;
    dut_end = mpz_cmp_ui(mod->rv32e$fetch$prevFinish, 1) == 0 && mpz_cmp_ui(mod->rv32e$writeback$prevInst, 0x00100073) == 0;
#ifdef DIFFTEST
    ref_cycle(1);
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed after " << cycles << " cycles\n";
      return 0;
    }
#endif
    if(dut_end) {
      clock_t dur = clock() - start;
      if(mpz_sgn(mod->rv32e$regs$regs_0) == 0){
          printf("\33[1;32mCPU HIT GOOD TRAP after %d cycles (%d ms, %d per sec)\033[0m\n", cycles, dur * 1000 / CLOCKS_PER_SEC, cycles * CLOCKS_PER_SEC / dur);
      }else{
          printf("\33[1;31mCPU HIT BAD TRAP after %d cycles\033[0m\n", cycles);
      }
    }
  }
}