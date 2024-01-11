#include <iostream>
#include <time.h>
#include <cstring>
#include <cassert>
#include <vector>
#include <numeric>
#include <algorithm>
#include <fstream>

#include <SimTop.h>
MOD_NAME* mod;

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

void mod_reset() {
  mod->set_reset(1);
  mod->step();
  mod->set_reset(0);
}

int main(int argc, char** argv) {
  load_program(argv[1]);
  mod = new MOD_NAME();
  memcpy(&mod->mem$rdata_mem$mem, program, program_sz);
  mod->set_io_logCtrl_log_begin(0);
  mod->set_io_logCtrl_log_end(0);
  mod->set_io_uart_in_ch(-1);
  mod_reset();

  std::cout << "start testing.....\n";
  bool dut_end = false;
  uint64_t cycles = 0;
  while(!dut_end) {
    cycles ++;
    mod->step();
    if (mod->io_uart_out_valid) {
      printf("%c", mod->io_uart_out_ch);
      fflush(stdout);
    }
  }
  return 0;
}