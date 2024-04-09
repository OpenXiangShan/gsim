#ifndef VALINFO_H
#define VALINFO_H

enum valStatus {VAL_EMPTY = 0, VAL_VALID, VAL_CONSTANT, VAL_FINISH /* for printf/assert*/ , VAL_INVALID, VAL_CONS_INVALID, VAL_EMPTY_SRC};

class valInfo {
private:
  mpz_t mask;
public:
  std::string valStr;
  int opNum = 0;
  valStatus status = VAL_VALID;
  std::vector<std::string> insts;
  mpz_t consVal;
  int width = 0;
  bool sign = 0;
  int consLength = 0;
  Node* splittedArray = nullptr;
  int beg = -1;
  int end = -1;
  std::vector<valInfo*> memberInfo;
  bool sameConstant = false;
  mpz_t assignmentCons;

  valInfo() {
    mpz_init(consVal);
    mpz_init(mask);
    mpz_init(assignmentCons);
  }
  void mergeInsts(valInfo* newInfo) {
    insts.insert(insts.end(), newInfo->insts.begin(), newInfo->insts.end());
    newInfo->insts.clear();
  }
  void setConsStr() {
    if (mpz_sgn(consVal) >= 0) {
      valStr = mpz_get_str(NULL, 16, consVal);
    } else {
      mpz_t sintVal;
      mpz_init(sintVal);
      u_asUInt(sintVal, consVal, widthBits(width));
      valStr = mpz_get_str(NULL, 16, sintVal);
    }
    consLength = valStr.length();
    if (valStr.length() <= 16) valStr = Cast(width, sign) + "0x" + valStr;
    else valStr = format("UINT128(0x%s, 0x%s)", valStr.substr(0, valStr.length() - 16).c_str(), valStr.substr(valStr.length()-16, 16).c_str());
    status = VAL_CONSTANT;
    mpz_set(assignmentCons, consVal);
    sameConstant = true;
  }
  void updateConsVal() {
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, width);
    mpz_sub_ui(mask, mask, 1);
    mpz_and(consVal, consVal, mask);
    setConsStr();
  }
  void setConstantByStr(std::string str, int base = 16) {
    mpz_set_str(consVal, str.c_str(), base);
    updateConsVal();
  }
  valInfo* dup(int beg = -1, int end = -1) {
    valInfo* ret = new valInfo();
    ret->opNum = opNum;
    ret->valStr = valStr;
    ret->status = status;
    mpz_set(ret->consVal, consVal);
    ret->width = width;
    ret->sign = sign;
    ret->consLength = consLength;
    if (beg < 0) {
      beg = 0;
      end = memberInfo.size() - 1;
    }
    for (int i = beg; i <= end; i ++) {
      if (getMemberInfo(i)) ret->memberInfo.push_back(getMemberInfo(i)->dup());
      else ret->memberInfo.push_back(nullptr);
    }
    return ret;
  }
  valInfo* getMemberInfo(size_t idx) {
    if (idx >= memberInfo.size()) return nullptr;
    return memberInfo[idx];
  }
};

#endif