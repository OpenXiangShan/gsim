#include <iostream>
#include <Exp1AllTest.h>

MOD_NAME* mod;

int main() {
  mod = new MOD_NAME();

  mod->set_reset(1);
  mod->step();

  mod->set_reset(0);
  while(!mod->finish) {
    mod->step();
  }
  std::cout << "Success!\n";
}