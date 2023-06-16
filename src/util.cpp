#include <string>
#include <map>
#include <iostream>

std::pair<int, std::string> strBase(std::string s) {
  if (s.length() <= 1) return std::make_pair(10, s);
  std::string ret;
  int idx = 1;
  int base = -1;
  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }
  if (s[0] == 'b') {
    if(s.length() - idx <= 64) ret += "0b";
    else base = 2;
  } else if(s[0] == 'o') {
    if(s.substr(idx) <= "1777777777777777777777") ret += "0";
    else base = 8;
  } else if(s[0] == 'h') {
    if(s.length() - idx <= 16 ) ret += "0x";
    else base = 16;
  } else {
    int decIdx = s[0] == '-';
    if(s.substr(decIdx) > "18446744073709551615") base = 10;
    idx = 0;
  }
  ret += s.substr(idx);
  return std::make_pair(base, ret);
}

std::pair<int, std::string> strBaseAll(std::string s) {
  if (s.length() <= 1) return std::make_pair(10, s);
  std::string ret;
  int idx = 1;
  int base = -1;
  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }
  if (s[0] == 'b') {
    base = 2;
  } else if(s[0] == 'o') {
    base = 8;
  } else if(s[0] == 'h') {
    base = 16;
  } else {
    base = 10;
    idx = 0;
  }
  ret += s.substr(idx);
  return std::make_pair(base, ret);
}

std::string to_hex_string(unsigned long x) {
  if (x == 0) {
    return "0";
  }
  std::string ret;
  while(x != 0) {
    int rem = x % 16;
    ret = (char)(rem < 10 ? (rem + '0') : (rem - 10 + 'a')) + ret;
    x /= 16;
  }
  return ret;
}