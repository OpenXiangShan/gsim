
#include <cstdio>
#include <vector>
#include <stack>
#include <set>
#include "common.h"
#define MAX_NODES_PER_SUPER 7000
#define MAX_SUBLINGS 30
#define MAX_NEAR_NUM 50

void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

bool anyPath(SuperNode* src, SuperNode* dst) {
  std::stack<SuperNode*> s;
  std::set<SuperNode*> visited;
  visited.insert(src);
  for (SuperNode* prev : src->depPrev) {
    s.push(prev);
    visited.insert(prev);
  }
  while (!s.empty()) {
    SuperNode* top = s.top();
    s.pop();
    if (top == dst) return true;
    for (SuperNode* prev : top->depPrev) {
      if (visited.find(prev) == visited.end()) {
        visited.insert(prev);
        s.push(prev);
      }
    }
  }
  return false;
}

void graph::mergeWhenNodes() {
/* each superNode contain one node */
  std::map<Node*, SuperNode*> whenMap;
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    Assert(super->member.size() <= 1, "invalid super size %ld", super->member.size());
    for (Node* member : super->member) {
      if (member->isArray() || member->assignTree.size() != 1) continue;
      if (member->assignTree[0]->getRoot()->opType != OP_WHEN) continue;
      Node* whenNode = member->assignTree[0]->getRoot()->getChild(0)->getNode();
      if (!whenNode) continue; // TODO: expr can also be optimized
      /* exists and no loop */
      if (whenMap.find(whenNode) == whenMap.end()) {
        whenMap[whenNode] = super;
        continue;
      }
      SuperNode* whenSuper = whenMap[whenNode];
      if (anyPath(super, whenSuper)) continue;
      /* merge member to whenSuper */
      whenSuper->member.push_back(member);
      member->super = whenSuper;
      super->member.clear();
      /* update super connection */
      whenSuper->addNext(super->next);
      for (SuperNode* next : super->next) {
        next->erasePrev(super);
        next->addPrev(whenSuper);
      }
      whenSuper->addPrev(super->prev);
      for (SuperNode* prev : super->prev) {
        prev->eraseNext(super);
        prev->addNext(whenSuper);
      }
    }
  }

  size_t prevSuper = sortedSuper.size();
  removeEmptySuper();
  reconnectSuper();
  detectSortedSuperLoop();
  printf("[mergeNodes-when] remove %ld superNodes (%ld -> %ld)\n", prevSuper - sortedSuper.size(), prevSuper, sortedSuper.size());
}

void graph::mergeAsyncReset() {
  std::map<Node*, SuperNode*> resetMap;
  for (Node* reg : regsrc) {
    if (reg->reset != ASYRESET) continue;
    std::set<Node*> prev;
    Assert(reg->resetTree->getRoot()->opType == OP_RESET, "invalid resetTree");
    getENodeRelyNodes(reg->resetTree->getRoot()->getChild(0), prev);
    Assert(prev.size() == 1, "multiple prevReset %s", reg->name.c_str());
    Node* condNode = *prev.begin();
    SuperNode* condSuper;
    if (resetMap.find(condNode) == resetMap.end()) {
      condSuper = condNode->super;
      condSuper->superType = SUPER_ASYNC_RESET;
      resetMap[condNode] = condSuper;
      condSuper->resetNode = condNode;
      condNode->setAsyncReset();
    } else {
      condSuper = resetMap[condNode];
    }
    Node* resetReg = reg->dup(NODE_REG_RESET, reg->name);
    Node* resetRegDst = reg->dup(NODE_REG_RESET, reg->getDst()->name);
    resetReg->regNext = reg;
    resetRegDst->regNext = reg;
    resetReg->assignTree.push_back(new ExpTree(reg->resetTree->getRoot(), resetReg));
    resetRegDst->assignTree.push_back(new ExpTree(reg->resetTree->getRoot(), resetRegDst));
    condSuper->add_member(resetReg);
    condSuper->add_member(resetRegDst);
  }
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
      bool canMerge = true;
      for (SuperNode* depNext : super->depNext) {
        if (depNext->order < nextSuper->order) canMerge = false;
      }
      if (!canMerge) continue;
      for (Node* member : super->member) member->super = nextSuper;
      /* move members in super to next super*/
      for (Node* member : super->member) {
        member->super = nextSuper;
      }
      /* update member */
      nextSuper->member.insert(nextSuper->member.begin(), super->member.begin(), super->member.end());
      /* update connection */
      nextSuper->erasePrev(super);
      nextSuper->addPrev(super->prev);
      for (SuperNode* prev : super->depPrev) {
        if (super->prev.find(prev) != super->prev.end()) {
          prev->eraseNext(super);
          prev->addNext(nextSuper);
          nextSuper->addPrev(prev);
        } else { // only in depPrev
          prev->eraseDepNext(super);
          prev->addDepNext(nextSuper);
          nextSuper->addDepPrev(prev);
        }
      }
      for (SuperNode* next : super->depNext) {
        if (super->next.find(next) == super->next.end()) { // only in depNext
          next->eraseDepPrev(super);
          next->addDepPrev(nextSuper);
          nextSuper->addDepNext(next);
        }
      }
      super->member.clear();
      super->clear_relation();
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
      bool canMerge = true;
      for (SuperNode* depPrev : super->depPrev) {
        if (depPrev->order > prevSuper->order) canMerge = false;
      }
      if (!canMerge) continue;
      /* move members in super to prev super */
      for (Node* member : super->member) member->super = prevSuper;
      Assert(prevSuper->member.size() != 0, "empty prevSuper %d", prevSuper->id);
      prevSuper->member.insert(prevSuper->member.end(), super->member.begin(), super->member.end());
      /* update connection */
      prevSuper->eraseNext(super);
      prevSuper->addNext(super->next);
      for (SuperNode* next : super->depNext) {
        if (super->next.find(next) != super->next.end()) {
          next->erasePrev(super);
          next->addPrev(prevSuper);
          prevSuper->addNext(next);
        } else { // only in depNext
          next->eraseDepPrev(super);
          next->addDepPrev(prevSuper);
          prevSuper->addDepNext(next);
        }
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
