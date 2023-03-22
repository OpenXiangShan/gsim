#ifndef PNODE_H
#define PNODE_H
#include "common.h"

enum {P_INVALID, P_CIRCUIT, P_CIR_MODS, P_MOD, P_EXTMOD, P_INTMOD, P_PORTS, P_INPUT, P_OUTPUT, \
      P_WIRE_DEF, P_INST, P_NODE, P_CONNECT, P_PAR_CONNECT, P_WHEN, P_MEMORY, P_READER, P_WRITER, P_READWRITER, \
      P_RUW, P_RLATENCT, P_WLATENCT, P_DEPTH, P_REF, P_REF_DOT, P_REF_IDX, P_EXPR_INT, \
      P_2EXPR, P_1EXPR, P_1EXPR1INT, P_1EXPR2INT, P_FIELD, P_FLIP_FIELD, P_AG_TYPE, P_AG_FIELDS, \
      P_Clock,  };

class PNode {
public:
  PNode() {

  }
  PNode(int _type) {
    type = _type;
  }
  std::vector<PNode*>child;
  std::string info;
  std::string name;
  std::vector<std::string> extraInfo;
  int type;
  int width;
  void appendChild(PNode* p) {
    child.push_back(p);
  }
  void appendExtraInfo(char* info) {
    extraInfo.push_back(std::string(info));
  }
  void appendChildList(PList* plist) {
    if(plist)
      parent->child.assign(plist->siblings.begin(), plist->siblings.end());
  }
  void setWidth(int _width) {
    width = _width;
  }
};

class PList{
public:
  PList(PNode* pnode) {
    siblings.push_back(pnode);
  }
  PList() {

  }
  std::vector<PNode*> siblings;
  void append(PNode* pnode) {
    if(pnode) siblings.append(pnode);
  }
  void append(int num, ...) {
    va_list valist;
    va_start(valist, num);
    for(int i = 0; i < num; i++){
      PNode* pnode = va_arg(valist, PNode*);
      if(pnode) siblings.append(pnode);
    }
    va_end(valist);
  }
}


#endif
