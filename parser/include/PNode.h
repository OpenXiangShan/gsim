#ifndef PNODE_H
#define PNODE_H
#include "common.h"

class PList;

enum {P_INVALID, P_CIRCUIT, P_CIR_MODS, P_MOD, P_EXTMOD, P_INTMOD, P_PORTS, P_INPUT, P_OUTPUT, \
      P_WIRE_DEF, P_REG_DEF, P_INST, P_NODE, P_CONNECT, P_PAR_CONNECT, P_WHEN, P_ELSE, \
      P_MEMORY, P_READER, P_WRITER, P_READWRITER, \
      P_RUW, P_RLATENCT, P_WLATENCT, P_DATATYPE, P_DEPTH, P_REF, P_REF_DOT, P_REF_IDX, P_EXPR_INT, \
      P_2EXPR, P_1EXPR, P_1EXPR1INT, P_1EXPR2INT, P_FIELD, P_FLIP_FIELD, P_AG_TYPE, P_AG_FIELDS, \
      P_Clock, P_INT_TYPE, P_EXPR_INT_NOINIT, P_EXPR_INT_INIT, P_EXPR_MUX, P_STATEMENTS};

class PNode {
public:
  PNode() {

  }
  PNode(int _type) {
    type = _type;
  }
  PNode(char* str) {
    name = std::string(str);
  }
  std::vector<PNode*>child;
  std::string info;
  std::string name;
  std::vector<std::string> extraInfo;
  int type;
  int width;
  bool sign;

  void appendChild(PNode* p);
  void appendExtraInfo(char* info);
  void appendChildList(PList* plist);
  void setWidth(int _width);
  int getChildNum();
  PNode* getChild(int idx);
  int getExtraNum() { return extraInfo.size(); }
  std::string getExtra(int idx) { return extraInfo[idx]; }
  void setSign(bool s) { sign = s;}
};

class PList{
public:
  PList(PNode* pnode) {
    siblings.push_back(pnode);
  }
  PList() {

  }
  std::vector<PNode*> siblings;
  void append(PNode* pnode);
  void append(int num, ...);
  void concat(PList* plist);
};

PNode* newNode(int type, char* info, char* name, int num, ...);
PNode* newNode(int type, char* name, int num, ...);
PNode* newNode(int type, char* info, char* name, PList* plist);
#endif
