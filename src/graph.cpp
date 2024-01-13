#include "common.h"

void graph::reconnectSuper() {
  for (SuperNode* super : sortedSuper) {
    super->prev.clear();
    super->next.clear();
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) member->constructSuperConnect();
  }
}

void graph::updateSuper() {
    /* remove deadNodes from superNodes */
  for (SuperNode* super : sortedSuper) {
    super->member.erase(
      std::remove_if(super->member.begin(), super->member.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
      super->member.end()
    );
  }
  /* remove empty superNodes */
  sortedSuper.erase(
    std::remove_if(sortedSuper.begin(), sortedSuper.end(), [](const SuperNode* sn) { return sn->member.size() == 0; }),
    sortedSuper.end()
  );

  reconnectSuper();
}

size_t graph::countNodes() {
  size_t ret = 0;
  for (SuperNode* super : sortedSuper) ret += super->member.size();
  return ret;
}

void graph::removeNodes(NodeStatus status) {
  for (SuperNode* super : sortedSuper) {
    super->member.erase(
      std::remove_if(super->member.begin(), super->member.end(), [status](const Node* n){ return n->status == status; }),
      super->member.end()
    );
  }

  removeEmptySuper();
  reconnectSuper();
}

void graph::removeEmptySuper() {
  sortedSuper.erase(
    std::remove_if(sortedSuper.begin(), sortedSuper.end(), [](const SuperNode* super) {return super->member.size() == 0; }),
    sortedSuper.end()
  );
}