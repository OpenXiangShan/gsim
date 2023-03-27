#include <cstdarg>
#include "PNode.h"

void PNode::appendChild(PNode* p) {
    child.push_back(p);
  }
void PNode::appendExtraInfo(char* info) {
  extraInfo.push_back(std::string(info));
}
void PNode::appendChildList(PList* plist) {
  if(plist)
    child.insert(child.end(), plist->siblings.begin(), plist->siblings.end());
}
void PNode::setWidth(int _width) {
  width = _width;
}

int PNode::getChildNum() {
  return child.size();
}

PNode* PNode::getChild(int idx) {
  return child[idx];
}

void pnewNode(PNode* parent, int num, va_list valist) {
  for(int i = 0; i < num; i++){
    PNode* next = va_arg(valist, PNode*);
    parent->child.push_back(next);
  }
}
PNode* newNode(int type, char* info, char* name, int num, ...){
    PNode* parent = new PNode(type);
    if(info) parent->info = std::string(info);
    if(name) parent->name = std::string(name);
    va_list valist;
    va_start(valist, num);
    pnewNode(parent, num, valist);
    va_end(valist);
    return parent;
}

PNode* newNode(int type, char* name, int num, ...) {
  PNode* parent = new PNode(type);
  if(name) parent->name = std::string(name);
  va_list valist;
  va_start(valist, num);
  pnewNode(parent, num, valist);
  va_end(valist);
  return parent;
}

PNode* newNode(int type, char* info, char* name, PList* plist) {
  PNode* parent = new PNode(type);
  if(info) parent->info = std::string(info);
  if(name) parent->name = std::string(name);
  if(plist)
    parent->child.assign(plist->siblings.begin(), plist->siblings.end());
  return parent;
}

void PList::append(PNode* pnode) {
  if(pnode) siblings.push_back(pnode);
}

void PList::append(int num, ...) {
  va_list valist;
  va_start(valist, num);
  for(int i = 0; i < num; i++){
    PNode* pnode = va_arg(valist, PNode*);
    if(pnode) siblings.push_back(pnode);
  }
  va_end(valist);
}
