/*
  merge regsrc into regdst if possible
*/

#include "common.h"
#include <stack>

void ExpTree::replace(Node* oldNode, Node* newNode) {
  std::stack<ENode*> s;
  if (getlval()) s.push(getlval());
  if (getRoot()) s.push(getRoot());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->nodePtr == oldNode) top->nodePtr = newNode;
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
}

void graph::mergeRegister() {
  int num = 0;
  for (Node* reg : regsrc) {
    bool spilt = false;
    for (Node* next : reg->next) {
      bool outSuperLater = next->super->order > reg->getDst()->super->order;
      bool inSuperLater = (next->super == reg->getDst()->super) && (next->super->findIndex(next) > reg->getDst()->super->findIndex(reg->getDst()));
      if (outSuperLater || inSuperLater) {
        spilt = true;
        break;
      }
    }
    if (!spilt) { // treat dst as NODE_OTHER
      num ++;
      reg->regSplit = reg->getDst()->regSplit = false;
      reg->getDst()->name = reg->name;
      reg->status = DEAD_SRC;
      /* replace nodes */
      // for (Node* next : reg->next) {
      //   for (ExpTree* tree : next->arrayVal) tree->replace(reg, reg->getDst());
      //   if (next->valTree) next->valTree->replace(reg, reg->getDst());
      // }
      /* update connection */
      for (Node* next : reg->next) {
        next->prev.erase(reg);
        next->prev.insert(reg->getDst());
      }
      reg->getDst()->next.insert(reg->next.begin(), reg->next.end());
      reg->prev.clear();
      reg->next.clear();
      /* update superNode connection */
      for (Node* next : reg->next) {
        next->super->prev.erase(reg->super);
        next->super->prev.insert(reg->getDst()->super);
      }
      reg->getDst()->super->next.insert(reg->super->next.begin(), reg->super->next.end());

      reg->getDst()->super->member.push_back(reg);
      reg->super = reg->getDst()->super;
    }
  }
  /* remove reg from regsrc */
  // regsrc.erase(
  //   std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* reg) { return reg->status == DEAD_NODE; } ),
  //   regsrc.end()
  // );
  /* remove superNodes and nodes */
  // removeNodes(DEAD_NODE);
  printf("[mergeRegister] merge %d (total %ld) registers\n", num, regsrc.size());
}