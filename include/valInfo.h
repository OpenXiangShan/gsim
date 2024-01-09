#ifndef VALINFO_H
#define VALINFO_H

enum valStatus {VAL_INVALID, VAL_VALID = 0, VAL_CONSTANT, VAL_FINISH /* for printf/assert*/};

class valInfo {
public:
  std::string valStr;
  int opNum = 0;
  valStatus status = VAL_VALID;
  std::vector<std::string> insts;
  mpz_t consVal;
  int width = 0;
  bool sign = 0;

  valInfo() {
    mpz_init(consVal);
  }
  void mergeInsts(valInfo* newInfo) {
    insts.insert(insts.end(), newInfo->insts.begin(), newInfo->insts.end());
  }
  void setConsStr() {
    valStr = mpz_get_str(NULL, 16, consVal);
    if (valStr.length() <= 16) valStr = "0x" + valStr;
    else valStr = format("UINT128(0x%s, 0x%s)", valStr.substr(0, valStr.length() - 16).c_str(), valStr.substr(valStr.length()-16, 16).c_str());
    status = VAL_CONSTANT;
  }
  void setConstantByStr(std::string str, int base = 16) {
    mpz_set_str(consVal, str.c_str(), base);
    setConsStr();
  }
  valInfo* dup() {
    valInfo* ret = new valInfo();
    ret->opNum = 0;
    ret->valStr = valStr;
    ret->status = status;
    mpz_set(ret->consVal, consVal);
    return ret;
  }
};

#endif