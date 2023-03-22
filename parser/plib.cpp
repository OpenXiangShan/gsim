#include "common.h"

static inline int bit(char c, int base) {
  switch (base) {
    case 2 : case 8: case 10: return c - '0';
    case 16: return (c >= '0' && c <= '9') ? c - '0' : 
                    ((c >= 'A' && c <= 'Z') ? c - 'A' : c - 'a');
    default: Assert("Invalid char %c with base %d\n", &c, base);
  }
}

int p_stoi(char* str) { // oct/dec/hex string to int
  int len = strlen(str);
  if (len  == 0) return 0;
  int val = 0;
  int idx = 1;
  int neg = 0;
  int base = 0;
  switch(str[0]) {
    case 'b': base = 2; break;
    case 'o': base = 8; break;
    case 'h': base = 16; break;
    default: base = 10; idx = 0;
  }
  if(str[idx] == '-') {
    idx ++;
    neg = 1;
  }
  for(int i = idx; i < len; i++) {
    val = val << base + bit(str[i], base);
  }
  if(neg) return -val;
  return val;
}

