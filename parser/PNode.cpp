/**
 * @file PNode.cpp
 * @brief PNode class functions implementions
 */

#include <cstdarg>
#include "common.h"

void PNode::appendChild(PNode* p) {
  if (p) child.push_back(p);
}
void PNode::appendExtraInfo(const char* info) { extraInfo.push_back(std::string(info)); }
void PNode::appendChildList(PList* plist) {
  if (plist) child.insert(child.end(), plist->siblings.begin(), plist->siblings.end());
}
void PNode::setWidth(int _width) { width = _width; }

int PNode::getChildNum() { return child.size(); }

PNode* PNode::getChild(int idx) {
  Assert(idx < (int)child.size(), "idx %d is outof bound(%ld)", idx, child.size()); return child[idx];
}

void pnewNode(PNode* parent, int num, va_list valist) {
  for (int i = 0; i < num; i++) {
    PNode* next = va_arg(valist, PNode*);
    parent->child.push_back(next);
  }
}

PNode* newNode(PNodeType type, int lineno, const char* info, const char* name, int num, ...) {
  PNode* parent = new PNode(type, lineno);
  if (info) parent->info = std::string(info);
  if (name) parent->name = std::string(name);
  va_list valist;
  va_start(valist, num);
  pnewNode(parent, num, valist);
  va_end(valist);
  return parent;
}

PNode* newNode(PNodeType type, int lineno, const char* name, int num, ...) {
  PNode* parent = new PNode(type, lineno);
  if (name) parent->name = std::string(name);
  va_list valist;
  va_start(valist, num);
  pnewNode(parent, num, valist);
  va_end(valist);
  return parent;
}

PNode* newNode(PNodeType type, int lineno, const char* info, const char* name, PList* plist) {
  PNode* parent = new PNode(type, lineno);
  if (info) parent->info = std::string(info);
  if (name) parent->name = std::string(name);
  if (plist) parent->child.assign(plist->siblings.begin(), plist->siblings.end());
  return parent;
}

void PList::append(PNode* pnode) {
  if (pnode) siblings.push_back(pnode);
}

void PList::append(int num, ...) {
  va_list valist;
  va_start(valist, num);
  for (int i = 0; i < num; i++) {
    PNode* pnode = va_arg(valist, PNode*);
    if (pnode) siblings.push_back(pnode);
  }
  va_end(valist);
}

void PList::concat(PList* plist) {
  if (!plist) return;
  siblings.insert(siblings.end(), plist->siblings.begin(), plist->siblings.end());
}
