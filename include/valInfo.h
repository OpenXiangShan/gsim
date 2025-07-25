#ifndef VALINFO_H
#define VALINFO_H

std::string legalCppCons(std::string str);
int upperPower2(int x);

enum valStatus {
  VAL_EMPTY = 0,
  VAL_VALID,
  VAL_CONSTANT,
  VAL_FINISH /* for printf/assert*/ ,
  VAL_INVALID
};
enum valType {TYPE_NORMAL = 0, TYPE_ARRAY, TYPE_STMT};
class valInfo {
private:
  mpz_t mask;
public:
  std::string valStr;
  int opNum = 0;
  valStatus status = VAL_VALID;
  valType type = TYPE_NORMAL;
  mpz_t consVal;
  int width = 0;
  bool sign = 0;
  int typeWidth = 0;
  int consLength = 0;
  int beg = -1;
  int end = -1;
  std::vector<valInfo*> memberInfo;
  bool sameConstant = false;
  mpz_t assignmentCons;
  bool directUpdate = true;

  valInfo(int _width = 0, bool _sign = 0) {
    mpz_init(consVal);
    mpz_init(mask);
    mpz_init(assignmentCons);
    width = _width;
    sign = _sign;
    typeWidth = upperPower2(_width);
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
    if (valStr.length() <= 16) valStr = (sign ? Cast(width, sign) : "") + "0x" + valStr;
    else valStr = legalCppCons(valStr);
    status = VAL_CONSTANT;
    mpz_set(assignmentCons, consVal);
    sameConstant = true;
    opNum = 0;
  }
  void updateConsVal() {
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, width);
    mpz_sub_ui(mask, mask, 1);
    mpz_and(consVal, consVal, mask);
    if (sign) {
      s_asSInt(consVal, consVal, width);
    }
    setConsStr();
  }
  void setConstantByStr(std::string str, int base = 16) {
    mpz_set_str(consVal, str.c_str(), base);
    updateConsVal();
  }
  valInfo* dup(int beg = 0, int end = -1) {
    valInfo* ret = new valInfo();
    ret->opNum = opNum;
    ret->type = type;
    ret->valStr = valStr;
    ret->status = status;
    mpz_set(ret->consVal, consVal);
    ret->width = width;
    ret->typeWidth = typeWidth;
    ret->sign = sign;
    ret->consLength = consLength;
    mpz_set(ret->assignmentCons, assignmentCons);
    ret->sameConstant = sameConstant;

    for (int i = beg; i <= end; i ++) {
      if (getMemberInfo(i)) ret->memberInfo.push_back(getMemberInfo(i)->dup());
      else ret->memberInfo.push_back(nullptr);
    }
    return ret;
  }

  valInfo* dupWithCons() {
    valInfo* ret = dup();
    mpz_set(ret->assignmentCons, assignmentCons);
    ret->sameConstant = sameConstant;
    return ret;
  }
  valInfo* getMemberInfo(size_t idx) {
    if (idx >= memberInfo.size()) return nullptr;
    return memberInfo[idx];
  }
};

#endif