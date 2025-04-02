#include "common.h"
#include <climits>
#include <cstddef>
#include <map>
#include <cstdio>
#include <queue>
#include <stack>
#include <map>
#include <tuple>

#define MAX_SIB_SIZE 25

std::map<SuperNode*, std::set<SuperNode*>> MFFC;
std::map<SuperNode*, SuperNode*> MFFCRoot;

void graph::MFFCPartition() {
/* computr MFFC */
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    SuperNode* super = sortedSuper[i];
    if (super->superType != SUPER_VALID) {
      MFFC[super] = std::set<SuperNode*>();
      MFFC[super].insert(super);
      MFFCRoot[super] = super;
      continue;
    }
    std::set<SuperNode*> SuperMFFC;
    SuperMFFC.insert(super);
    std::queue<SuperNode*> q;
    for (SuperNode* prev : super->prev) q.push(prev);
    while (!q.empty()) {
      SuperNode* top = q.front();
      q.pop();
      if (top->superType != SUPER_VALID) continue;
      bool allInMFFC = true;
      for (SuperNode* next : top->next) {
        if (SuperMFFC.find(next) == SuperMFFC.end()) {
          allInMFFC = false;
          break;
        }
      }
      if (allInMFFC) {
        SuperMFFC.insert(top);
        for (SuperNode* prev : top->prev) q.push(prev);
      }
    }
    MFFC[super] = SuperMFFC;
    SuperNode* root = super;
    for (SuperNode* node : SuperMFFC) {
      if (node->order < root->order) root = node;
    }
    for (SuperNode* node : SuperMFFC) MFFCRoot[node] = root;
  }

/* merge */
  for (SuperNode* super : sortedSuper) {
    SuperNode* root = MFFCRoot[super];
    if (root == super) continue;
    for (Node* node : super->member) {
      node->super = root;
      root->member.push_back(node);
    }
    super->member.clear();
  }
  size_t totalSuper = sortedSuper.size();
  removeEmptySuper();
  reconnectSuper();
  printf("[MFFCPartition] remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());
}

bool anyPath(SuperNode* super1, SuperNode* super2, graph* g) {
  std::stack<SuperNode*> s;
  s.push(super1);
  clock_t start = clock();
  while(!s.empty()) {
    clock_t end = clock();
    if((end - start) / CLOCKS_PER_SEC > 600) {
      Assert(0, "time error");
    }
    SuperNode* top = s.top();
    s.pop();
    if (top == super2) return true;
    for (SuperNode* next : top->next) s.push(next);
  }
  return false;
}

void findAllNext(std::set<SuperNode*>&next, SuperNode* super) {
  std::stack<SuperNode*> s;
  std::set<SuperNode*> visited;
  s.push(super);
  clock_t start = clock();
  while(!s.empty()) {
    clock_t end = clock();
    if((end - start) / CLOCKS_PER_SEC > 60) {
      Assert(0, "time error");
    }
    SuperNode* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    next.insert(top);
    visited.insert(top);
    for (SuperNode* nextNode : top->next) s.push(nextNode);
  }
}
void findAllPrev(std::set<SuperNode*>&prev, SuperNode* super) {
  std::stack<SuperNode*> s;
  s.push(super);
  std::set<SuperNode*> visited;
  clock_t start = clock();
  while(!s.empty()) {
    clock_t end = clock();
    if((end - start) / CLOCKS_PER_SEC > 60) {
      Assert(0, "time error");
    }
    SuperNode* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    prev.insert(top);
    visited.insert(top);
    for (SuperNode* prevNode : top->prev) s.push(prevNode);
  }
}

void graph::mergeEssentSmallSubling(size_t maxSize, double sim) {
  int idx = 0;
  int seg = sortedSuper.size() / 10;
  int percent = 0;
  for (SuperNode* super : sortedSuper) {
    idx ++;
    if (idx >= seg) {
      printf("%d0%%\n", percent ++);
      idx = 0;
    }
    if (super->prev.size() == 0 || super->member.size() > MAX_SIB_SIZE) continue;
    if (super->superType != SUPER_VALID) continue;
    std::set<SuperNode*> siblings;
    SuperNode* select = nullptr;
    double similarity = 0;
    for (SuperNode* prev : super->prev) {
      for (SuperNode* next : prev->next) {
        if (next->superType != SUPER_VALID) continue;
        if (next->member.size() <= maxSize && next->member.size() > 0 && next->order < super->order) {
          siblings.insert(next);
        }
      }
    }
    std::set<SuperNode*> next;
    findAllNext(next, super);
    std::set<SuperNode*> prev;
    findAllPrev(prev, super);
    for (SuperNode* sib : siblings) {
      if (next.find(sib) != next.end() || prev.find(sib) != prev.end()) continue;
      int num = 0;
      for (SuperNode* prev : super->prev) {
        if (sib->prev.find(prev) != sib->prev.end()) num ++;
      }
      double new_sim = (double)num / super->prev.size();
      if (new_sim > sim) {
        if (!select || new_sim > similarity) {
          select = sib;
          similarity = new_sim;
        }
      }
    }
    if (select) {
      for (Node* node : super->member) {
        node->super = select;
        select->member.push_back(node);
      }
      super->member.clear();
      /* update connect */
      for (SuperNode* prev : super->prev) {
        prev->eraseNext(super);
        prev->addNext(select);
        select->addPrev(prev);
      }
      for (SuperNode* next : super->next) {
        next->erasePrev(super);
        next->addPrev(select);
        select->addNext(next);
      }
      super->clear_relation();
    }

  }
  removeEmptySuper();
  reconnectSuper();
}

bool sortOrderCmp(Node* n1, Node* n2) {
  return n1->order < n2->order;
}


void graph::essentPartition(){
  size_t totalSuper = sortedSuper.size();
  size_t phaseSuper = sortedSuper.size();
  mergeAsyncReset();
  mergeUIntReset();
  printf("[mergeNodes-reset] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  orderAllNodes();
  MFFCPartition();
  for (SuperNode* super : sortedSuper) {
    std::sort(super->member.begin(), super->member.end(), sortOrderCmp);
  }
  resort();
  printf("[mergeNodes-MFFC] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeIn1();
  printf("[mergeNodes-In] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  mergeSublings();
  printf("[mergeNodes-Sibling] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

  phaseSuper = sortedSuper.size();
  orderAllNodes();
  mergeEssentSmallSubling(MAX_SIB_SIZE, 0.5);

  for (SuperNode* super : sortedSuper) {
    std::sort(super->member.begin(), super->member.end(), sortOrderCmp);
  }
  resort();

  printf("[mergeNodes-smallSib1] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());
  phaseSuper = sortedSuper.size();
  mergeEssentSmallSubling(4 * MAX_SIB_SIZE, 0.25);
  for (SuperNode* super : sortedSuper) {
    std::sort(super->member.begin(), super->member.end(), sortOrderCmp);
  }
  resort();

  printf("[mergeNodes-smallSib2] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());
  phaseSuper = sortedSuper.size();
  printf("[mergeNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - phaseSuper, totalSuper, phaseSuper);
}
