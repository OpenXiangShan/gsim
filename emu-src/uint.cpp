#include <stdio.h>
#include "uint.h"

void uint512_t::operator = (uint1024_t data) {
  // uint512_t ret;
  u256_1 = data.u512_0.u256_1;
  u256_0 = data.u512_0.u256_0;
  // return ret;
}