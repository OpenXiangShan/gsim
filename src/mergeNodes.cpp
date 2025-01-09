
#include <cstdio>
#include <vector>
#include "common.h"
#define MAX_NODES_PER_SUPER 7000
#define MAX_SUBLINGS 30
#define MAX_NEAR_NUM 50

void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

void graph::mergeAsyncReset() {
  std::map<Node*, SuperNode*> resetMap;
  for (int i = 0; i < sortedSuper.size(); i++) {
    for (Node* member: sortedSuper[i]->member) {
      if (member->type != NODE_REG_SRC || member->reset != ASYRESET) continue;
      if (member->assignTree.size() != 1) continue;
      Assert(member->assignTree[0]->getRoot()->opType == OP_RESET, "reset not found %s", member->name.c_str());
      std::set<Node*> resetNoeds;
      getENodeRelyNodes(member->assignTree[0]->getRoot()->getChild(0), resetNoeds);
      Assert(resetNoeds.size() == 1, "multiple reset %s", member->name.c_str());
      bool resetConstant = true;
      for (Node* prev : member->prev) {
        if (resetNoeds.find(prev) == resetNoeds.end() && prev->status != CONSTANT_NODE) resetConstant = false;
      }
      if (!resetConstant) continue;
      /* if resetVal is constant (using prev.size() == 1, TODO: optimize) */
      if (member->super->member.size() != 1) member->super->display();
      Assert(member->super->member.size() == 1, "super already merged %s id %d (size = %ld)", member->name.c_str(), member->super->id, member->super->member.size());
      Node* resetNode = *(resetNoeds.begin());
      resetNode->setAsyncReset();
      SuperNode* resetSuper;
      if (resetMap.find(resetNode) == resetMap.end()) {
        resetSuper = member->super;
        resetSuper->superType = SUPER_ASYNC_RESET;
        resetSuper->resetNode = resetNode;
        resetMap[resetNode] = resetSuper;
      } else {
        resetSuper = resetMap[resetNode];
        member->super = resetSuper;
        resetSuper->member.push_back(member);
        sortedSuper[i]->member.clear();
      }
    }
  }
  removeEmptySuper();
  reconnectSuper();
}

void graph::mergeUIntReset() {
  std::map<Node*, SuperNode*> resetSuper;
  for (Node* reg : regsrc) {
    if (reg->reset != UINTRESET) continue;
    std::set<Node*> prev;
    if (reg->resetTree->getRoot()->opType == OP_RESET) {
      getENodeRelyNodes(reg->resetTree->getRoot()->getChild(0), prev);
    } else {
      Panic();
      reg->resetTree->getRelyNodes(prev);
    }
    if (prev.size() != 1) reg->display();
    Assert(prev.size() == 1, "multiple prevReset %s", reg->name.c_str());
    Node* prevNode = *prev.begin();
    SuperNode* prevSuper;
    if (resetSuper.find(prevNode) == resetSuper.end()) {
      prevSuper = new SuperNode();
      prevSuper->superType = SUPER_UINT_RESET;
      resetSuper[prevNode] = prevSuper;
      uintReset.push_back(prevSuper);
      prevSuper->resetNode = prevNode;
      prevNode->setUIntReset();
    } else {
      prevSuper = resetSuper[prevNode];
    }
    prevSuper->member.push_back(reg);
  }
}
/*
  merge nodes with out-degree=1 to their successors
*/
void graph::mergeOut1() {
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    SuperNode* super = sortedSuper[i];
    /* do not merge superNodes that contains reg src */
    if (super->superType != SUPER_VALID) continue;
    if (super->next.size() == 1) {
      SuperNode* nextSuper = *(super->next.begin());
      if (nextSuper->superType != SUPER_VALID) continue;
      if (nextSuper->member.size() > MAX_NODES_PER_SUPER) continue;
      for (Node* member : super->member) member->super = nextSuper;
      /* move members in super to next super*/
      for (Node* member : super->member) {
        member->super = nextSuper;
      }

      nextSuper->member.insert(nextSuper->member.begin(), super->member.begin(), super->member.end());
      /* update connection */
      nextSuper->prev.erase(super);
      nextSuper->prev.insert(super->prev.begin(), super->prev.end());
      for (SuperNode* prevSuper : super->prev) {
        prevSuper->next.erase(super);
        prevSuper->next.insert(nextSuper);
      }
      super->member.clear();
    }
  }
  removeEmptySuper();
}

/*
  merge nodes with in-degree=1 to their preceding nodes
*/
void graph::mergeIn1() {
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    SuperNode* super = sortedSuper[i];
    if (super->superType != SUPER_VALID) continue;
    if (super->prev.size() == 1) {
      SuperNode* prevSuper = *(super->prev.begin());
      if (prevSuper->superType != SUPER_VALID) continue;
      if (prevSuper->member.size() > MAX_NODES_PER_SUPER) continue;
      /* move members in super to prev super */
      for (Node* member : super->member) member->super = prevSuper;
      Assert(prevSuper->member.size() != 0, "empty prevSuper %d", prevSuper->id);
      prevSuper->member.insert(prevSuper->member.end(), super->member.begin(), super->member.end());
      /* update connection */
      prevSuper->next.erase(super);
      prevSuper->next.insert(super->next.begin(), super->next.end());
      for (SuperNode* nextSuper : super->next) {
        nextSuper->prev.erase(super);
        nextSuper->prev.insert(prevSuper);
      }
      super->member.clear();
    }
  }

  removeEmptySuper();
  // reconnectSuper();
}

uint64_t prevHash(SuperNode* super) {
  uint64_t ret = super->prev.size()*7;
  for (SuperNode* prev : super->prev) {
    ret += (uint64_t)prev;
  }
  return ret;
}

bool prevEq(SuperNode* super1, SuperNode* super2) {
  return super1->prev == super2->prev;
}

void graph::mergeSublings() {
  std::map<uint64_t, std::vector<SuperNode*>> prevSuper;
  for (SuperNode* super : sortedSuper) {
    if (super->prev.size() == 0) continue;
    if (super->superType != SUPER_VALID) continue;
    uint64_t id = prevHash(super);
    if (prevSuper.find(id) == prevSuper.end()) prevSuper[id] = std::vector<SuperNode*>();
    prevSuper[id].push_back(super);
  }

  for (auto iter : prevSuper) {
    std::set<SuperNode*> uniquePrev;
    for (SuperNode* super : iter.second) {
      bool find = false;
      for (SuperNode* checkSuper : uniquePrev) {
        if (prevEq(checkSuper, super)) { // merge or replace
          find = true;
          if (checkSuper->member.size() < MAX_SUBLINGS) {
            for (Node* member : super->member) member->super = checkSuper;
            checkSuper->member.insert(checkSuper->member.end(), super->member.begin(), super->member.end());
            super->member.clear();
          } else {
            uniquePrev.erase(checkSuper);
            uniquePrev.insert(super);
          }
          break;
        }
      }
      if (!find) uniquePrev.insert(super);
    }
  }
  removeEmptySuper();
  reconnectSuper();
}

void graph::mergeNear() {
  for (size_t i = 1; i < sortedSuper.size(); i ++) {
    SuperNode* prevSuper = sortedSuper[i - 1];
    SuperNode* curSuper = sortedSuper[i];
    if (prevSuper->superType != SUPER_VALID || curSuper->superType != SUPER_VALID) continue;
    if (prevSuper->member.size() + curSuper->member.size() <= MAX_NEAR_NUM) {
      for (Node* node : prevSuper->member) {
        node->super = curSuper;
      }
      curSuper->member.insert(curSuper->member.begin(), prevSuper->member.begin(), prevSuper->member.end());
      prevSuper->member.clear();
    }
  }
  removeEmptySuper();
  reconnectSuper();
}

void graph::mergeNodes() {
  size_t totalSuper = sortedSuper.size();
  size_t phaseSuper = sortedSuper.size();

  mergeAsyncReset();
  mergeUIntReset();
  printf("[mergeNodes-reset] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeOut1();
  printf("[mergeNodes-out1] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeIn1();
  printf("[mergeNodes-in1] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeSublings();
  printf("[mergeNodes-subling] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeNear();
  printf("[mergeNodes-near-%d] remove %ld superNodes (%ld -> %ld)\n", MAX_NEAR_NUM, phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  printf("[mergeNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - phaseSuper, totalSuper, phaseSuper);

}