#include <cstdarg>
#include "PNode.h"

PNode* newNode(int type, std::string info, std::string name, int num, ...){
    PNode* parent = new PNode();
    parent->type = type;
    parent->info = std::string(info);
    parent->name = std::string(name);
    va_list valist;
    va_start(valist, num);
    for(int i = 0; i < num; i++){
        PNode* next = va_arg(valist, PNode*);
        parent->child.push_back(next);
    }
    va_end(valist);
    return parent;
}

PNode* newNode(int type, std::string info, PList* plist) {
  PNode* parent = new PNode();
  parent->type = type;
  parent->info = std::string(info);
  if(plist)
    parent->child.assign(plist->siblings.begin(), plist->siblings.end());
  return parent;
}

