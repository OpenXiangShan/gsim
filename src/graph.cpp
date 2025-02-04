#include "common.h"

void graph::reconnectAll() {
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->updateConnect();
    }
  }
  reconnectSuper();
}

void graph::reconnectSuper() {
  for (SuperNode* super : sortedSuper) {
    super->prev.clear();
    super->next.clear();
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) member->constructSuperConnect();
  }
}

void graph::removeNodesNoConnect(NodeStatus status) {
  for (SuperNode* super : sortedSuper) {
    super->member.erase(
      std::remove_if(super->member.begin(), super->member.end(), [status](const Node* n){ return n->status == status; }),
      super->member.end()
    );
  }
  removeEmptySuper();
}

size_t graph::countNodes() {
  size_t ret = 0;
  for (SuperNode* super : sortedSuper) ret += super->member.size();
  return ret;
}

void graph::removeNodes(NodeStatus status) {
  removeNodesNoConnect(status);
  removeEmptySuper();
  reconnectSuper();
}

void graph::removeEmptySuper() {
  sortedSuper.erase(
    std::remove_if(sortedSuper.begin(), sortedSuper.end(), [](const SuperNode* super) {return (super->superType == SUPER_VALID || super->superType == SUPER_UPDATE_REG) && super->member.size() == 0; }),
    sortedSuper.end()
  );
  for (int i = 0; i < THREAD_NUM; i ++) {
    parallelSuper[i].erase(
      std::remove_if(parallelSuper[i].begin(), parallelSuper[i].end(), [](const SuperNode* super) {return (super->superType == SUPER_VALID || super->superType == SUPER_UPDATE_REG) && super->member.size() == 0; }),
      parallelSuper[i].end()
    );
  }
}
