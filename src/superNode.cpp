#include "common.h"

int SuperNode::counter = 1;

void SuperNode::clear_relation() {
  prev.clear();
  next.clear();
  depPrev.clear();
  depNext.clear();
}

void SuperNode::addPrev(SuperNode* node) {
  prev.insert(node);
  depPrev.insert(node);
}

void SuperNode::addPrev(std::set<SuperNode*>& super) {
  prev.insert(super.begin(), super.end());
  depPrev.insert(super.begin(), super.end());
}

void SuperNode::erasePrev(SuperNode* node) {
  prev.erase(node);
  depPrev.erase(node);
}

void SuperNode::addNext(SuperNode* node) {
  next.insert(node);
  depNext.insert(node);
}

void SuperNode::addNext(std::set<SuperNode*>& super) {
  next.insert(super.begin(), super.end());
  depNext.insert(super.begin(), super.end());
}

void SuperNode::eraseNext(SuperNode* node) {
  next.erase(node);
  depNext.erase(node);
}
