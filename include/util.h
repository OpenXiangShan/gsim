#ifndef UTIL_H
#define UTIL_H

OPType str2op_expr2(std::string name);
OPType str2op_expr1(std::string name);
OPType str2op_expr1int1(std::string name);

int upperPower2(int x);
int upperLog2(int x);

std::string to_hex_string(BASIC_TYPE x);
std::pair<int, std::string> firStrBase(std::string s);

#endif
