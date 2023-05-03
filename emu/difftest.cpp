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

bool checkSignals(bool display) {
  #include "../obj/checkSig.h"
}

int main() {
  mod = new MOD_NAME();
  ref = new REF_NAME();
  ref_reset();

  for(int i = 0; i < 10; i++) {
    ref_cycle(1);
    mod->step();
    bool isDiff = checkSignals(false);
    if(isDiff) {
      std::cout << "all Sigs:\n -----------------\n";
      checkSignals(true);
      std::cout << "Failed!!\n";
      return 0;
    }
  }
}