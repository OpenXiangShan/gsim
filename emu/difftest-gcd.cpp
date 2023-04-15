#include <iostream>
#include <top.h>
#include "verilated.h"
#include HEADER

MOD_NAME* mod;
REF_NAME* ref;

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

void check(MOD_NAME* mod, REF_NAME* ref) {
  if((mpz_get_ui(mod->io_outputValid) != ref->io_outputValid) || (mpz_get_ui(mod->io_outputGCD) != ref->io_outputGCD))
    std::cout << "fail (" << mpz_get_ui(mod->io_outputValid) << ","<< mpz_get_ui(mod->io_outputGCD) << ") (" << (int)ref->io_outputValid << "," << ref->io_outputGCD << ")" << std::endl;
  else
    std::cout << "pass!\n";
}

int main() {
  mod = new MOD_NAME();
  ref = new REF_NAME();
  ref_reset();
  mpz_t val;
  mpz_init(val);
  for (int i = 2; i < 10; i++) {
    mpz_set_ui(val, i*21);
    mod->set_io_value1(val);
    mpz_set_ui(val, i+1);
    mod->set_io_value2(val);
    mpz_set_ui(val, 1);
    mod->set_io_loadingValues(val);
    ref->io_value1 = i * 21;
    ref->io_value2 = i+1;
    ref->io_loadingValues=1;
    mod->step();
    ref_cycle(1);
    mpz_set_ui(val, 0);
    mod->set_io_loadingValues(val);
    ref->io_loadingValues = 0;
    while(!ref->io_outputValid) {
      mod->step();
      std::cout << "(" << mpz_get_ui(mod->io_outputValid) << ","<< mpz_get_ui(mod->io_outputGCD) << ") (" << (int)ref->io_outputValid << "," << ref->io_outputGCD << ")" << std::endl;
      ref_cycle(1);

    }
    mod->step();
    check(mod, ref);
  }
}