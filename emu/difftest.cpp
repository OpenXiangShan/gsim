#include <iostream>
#include <top.h>
#include "verilated.h"
#include "Vaddreg__Syms.h"

Saddreg* mod;
Vaddreg* ref;

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

int main() {
  mod = new Saddreg();
  ref = new Vaddreg();
  ref_reset();
  for (int i = 1; i < 10; i++) {
    mod->set_io_a(i);
    mod->set_io_b(i+1);
    mod->step();
    ref->io_a = i;
    ref->io_b = i+1;
    ref->eval();
    std::cout << mod->io_result << " " << ref->io_result << std::endl;
    ref_cycle(1);
  }
}