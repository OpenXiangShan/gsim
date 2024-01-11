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
