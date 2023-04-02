#include <iostream>
#include <top.h>
int main() {
  Saddreg* mod = new Saddreg();
  for (int i = 1; i < 10; i++) {
    mod->set_io_a(i);
    mod->set_io_b(i+1);
    mod->step();
    std::cout << mod->io_result << std::endl;
  }
}