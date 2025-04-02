#include "common.h"
#include <climits>
#include <cstddef>
#include <map>
#include <cstdio>
#include <queue>
#include <stack>
#include <map>
#include <tuple>

// #define SUPER_BOUND 35

void graph::resort() {
  std::map<SuperNode*, int>times;
  std::stack<SuperNode*> s;
  std::set<SuperNode*> visited;
  std::vector<SuperNode*> prevSuper(sortedSuper);

  size_t prevSize = sortedSuper.size();
  for (SuperNode* node : sortedSuper) {
    if (node->depPrev.size() == 0) s.push(node);
    times[node] = 0;
  }
  sortedSuper.clear();

  while(!s.empty()) {
    SuperNode* top = s.top();
    s.pop();
    Assert(visited.find(top) == visited.end(), "superNode %d is already visited\n", top->id);
    visited.insert(top);
    sortedSuper.push_back(top);
#ifdef ORDERED_TOPO_SORT
    std::vector<SuperNode*> sortedNext;
    sortedNext.insert(sortedNext.end(), top->depNext.begin(), top->depNext.end());
    std::sort(sortedNext.begin(), sortedNext.end(), [](SuperNode* a, SuperNode* b) {return a->id < b->id;});
    for (SuperNode* next : sortedNext) {
#else
    for (SuperNode* next : top->depNext) {
#endif
      times[next] ++;
      if (times[next] == (int)next->depPrev.size()) s.push(next);
    }
  }

  Assert(sortedSuper.size() == prevSize, "invalid size %ld %ld\n", sortedSuper.size(), prevSize);
  orderAllNodes();
}

// coarsen phase
void graph::graphCoarsen() {

  mergeAsyncReset();
  mergeUIntReset();

  mergeWhenNodes();
  resort();

  mergeOut1();
  mergeIn1();
  mergeSublings();
}

// initial partition

void graph::graphInitPartition() {
  printf("[graphPartition] Setting the maximum size of a superNode to %d\n", globalConfig.SuperNodeMaxSize);
  /*
  Kernighan’s algorithm: new part start at x
  * cost：C(x) = sum(i<x<=j)(cij)
  * T(x): partial cost 
    - T(1) = 0
    - T(x) = C(x) + T(y)
  * dynamic programming
  */
  std::vector<int> T(sortedSuper.size() + 1, INT_MAX);
  std::vector<int> b(sortedSuper.size() + 1, -1); // backtrace
  std::vector<int> C(sortedSuper.size() + 1, 0);
  std::vector<int> internal(sortedSuper.size() + 1, 0);
  /* compute C */
  for (size_t idx = 0; idx < sortedSuper.size(); idx ++) {
    for (SuperNode* next : sortedSuper[idx]->next) {
      // printf("edge %d -> %d\n", sortedSuper[idx]->order, next->order);
      for (int i = sortedSuper[idx]->order + 1; i <= next->order; i ++) {
        C[i] ++;
      }
      internal[next->order] ++;
    }
  }
  for (size_t i = 1; i < internal.size(); i ++) internal[i] += internal[i - 1];
  // for (int i = 0; i < sortedSuper.size(); i ++) printf("C[%d] = %d internal[%d] = %d\n", i, C[i], i, internal[i]);
  T[0] = 0;
  /* compute T by dynamic programming */
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    // printf("T[%ld] = %d size %ld\n", i, T[i], sortedSuper[i]->member.size());
    size_t nextBound = i + 1;
    size_t accuCost = sortedSuper[i]->member.size();
    for (; nextBound < sortedSuper.size() && accuCost + sortedSuper[nextBound]->member.size() <= globalConfig.SuperNodeMaxSize; nextBound ++) {
      accuCost += sortedSuper[nextBound]->member.size();
    }
    /* update T[i + 1] to T[nextBound] that jmp at i */
    int Cij = 0;
    for (size_t j = i + 1; j <= nextBound; j ++) {
      Cij += sortedSuper[j - 1]->next.size();
      for (SuperNode* prev : sortedSuper[j - 1]->prev) {
        if (prev->order >= (int)i) Cij --;
      }
      int newT = T[i] + Cij;
      if(T[j] > newT) {
        T[j] = newT;
        b[j] = i;
      }
    }
  }
  std::set<int> cut;
  int idx = sortedSuper.size();
  while (b[idx] > 0) {
    cut.insert(b[idx]);
    idx = b[idx];
  }
  cut.insert(sortedSuper.size());
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    if (sortedSuper[i]->superType != SUPER_VALID) {
      cut.insert(i);
      if (i + 1 < sortedSuper.size()) cut.insert(i + 1);
    }
  }
  // for (int i : cut) printf("cut %d\n", i);
  
  int cutBeg = 0;
  for (int cutEnd : cut) {
    SuperNode* master = sortedSuper[cutBeg];
    for (int i = cutBeg + 1; i < cutEnd; i ++) {
      SuperNode* memberSuper = sortedSuper[i];
      for (Node* node : memberSuper->member) {
        node->super = master;
      }
      master->member.insert(master->member.end(), memberSuper->member.begin(), memberSuper->member.end());
      memberSuper->member.clear();
    }
    cutBeg = cutEnd;
  }
  removeEmptySuper();
  reconnectSuper();
}


#define REFINE_TYPE std::tuple<Node*, SuperNode*, int>
#define REFINE_NODE(gain) std::get<0>(gain)
#define REFINE_DST(gain) std::get<1>(gain)
#define REFINE_GAIN(gain) std::get<2>(gain)
struct gainLess { // ascending order
  bool operator()(REFINE_TYPE n1, REFINE_TYPE n2) {
    return REFINE_GAIN(n1) < REFINE_GAIN(n2);
  }
};
static std::priority_queue<REFINE_TYPE, std::vector<REFINE_TYPE>, gainLess> gainQueue;
static std::vector<Node*> allGainNodes;
static std::map<Node*, std::pair<SuperNode*, int>> incomingMap;
static std::map<Node*, std::pair<SuperNode*, int>> outcomingMap;
static std::map<Node*, int> anyExtEdge;

void addGainQueue(Node* node, SuperNode* dst, int gain) {
  gainQueue.push(std::make_tuple(node, dst, gain));
}

void updateExtEdge(Node* node) {
  for (Node* next : node->next) {
    if (node->super != next->super) {
      anyExtEdge[node] = 1;
      break;
    }
  }
}

int nodeGain(Node* node, SuperNode* dst) {
/* number of edge cut */
  Assert(node->super != dst, "invalid gain %s\n", node->name.c_str());
  int gain = 0;
  for (Node* prev : node->prev) {
    if (prev->super == dst) gain ++;
    else if (prev->super == node->super) gain --;
  }
  for (Node* next : node->next) {
    if (next->super == dst) gain ++;
    else if (next->super == node->super) gain --;
  }

/* number of nodes that has extEdge */
  int boundaryNum = 0;
  /* prev may become boundary */
  for (Node* prev : node->prev) {
    if (node->super == prev->super && anyExtEdge[prev] == 0) boundaryNum ++;
  }
  bool nodeIsBoundary = false;
  for (Node* next : node->next) {
    if (node->super != next->super) {
      nodeIsBoundary = true;
    }
  }
  if (nodeIsBoundary) boundaryNum --;
  gain -= boundaryNum * 2;
  return gain;
}

REFINE_TYPE popGainQueue() {
  REFINE_TYPE ret = gainQueue.top();
  gainQueue.pop();
  return ret;
}

bool validGainEmpty() {
  while (!gainQueue.empty()) {
    REFINE_TYPE ret = gainQueue.top();
    if (incomingMap.find(REFINE_NODE(ret)) != incomingMap.end()
        && incomingMap[REFINE_NODE(ret)].first == REFINE_DST(ret)
        && incomingMap[REFINE_NODE(ret)].second == REFINE_GAIN(ret)) {
      break;
    }
    if (outcomingMap.find(REFINE_NODE(ret)) != outcomingMap.end()
        && outcomingMap[REFINE_NODE(ret)].first == REFINE_DST(ret)
        && outcomingMap[REFINE_NODE(ret)].second == REFINE_GAIN(ret)) {
      break;
    }
    popGainQueue();
  }
  return gainQueue.empty();
}

SuperNode* checkIncoming(Node* node) {
  bool isInComing = true;
  Node* maxNode = nullptr;
  int maxOrder = -1;
  for (Node* prev : node->prev) {
    if (prev->super->order == node->super->order) {
      isInComing = false;
      break;
    } else {
      if (prev->super->order > maxOrder) {
        maxOrder = prev->super->order;
        maxNode = prev;
      }
    }
  }
  if (isInComing && maxNode && maxNode->super->superType == SUPER_VALID) return maxNode->super;
  return nullptr;
}

SuperNode* checkOutcoming(Node* node) {
  bool isOutComing = true;
  Node* minNode = nullptr;
  int minOrder = -1;
  for (Node* next : node->next) {
    if (next->super->order == node->super->order) {
      isOutComing = false;
      break;
    } else {
      if (next->super->order < minOrder) {
        minOrder = next->super->order;
        minNode = next;
      }
    }
  }
  if (isOutComing && minNode && minNode->super->superType == SUPER_VALID) return minNode->super;
  return nullptr;
}

void addIncoming(Node* node, SuperNode* incomingNode) {
  int gain = nodeGain(node, incomingNode);
  addGainQueue(node, incomingNode, gain);
  incomingMap[node] = std::make_pair(incomingNode, gain);
}

void addOutcoming(Node* node, SuperNode* outcomingNode) {
  int gain = nodeGain(node, outcomingNode);
  addGainQueue(node, outcomingNode, gain);
  outcomingMap[node] = std::make_pair(outcomingNode, gain);
}

// refine & uncoarsen phase
void graph::graphRefine() {
  /* initial gain for each incoming and outcoming nodes */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      allGainNodes.push_back(node);
    }
  }
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    for (Node* node : super->member) {
      /* check incoming */
      SuperNode* incomingNode = checkIncoming(node);
      if (incomingNode) addIncoming(node, incomingNode);
      /* check outcoming */
      SuperNode* outcomingNode = checkOutcoming(node);
      if (outcomingNode) addOutcoming(node, outcomingNode);
    }
  }
  /* move */
  int refineNum = 0;
  while (!validGainEmpty()) {
    refineNum ++;
    Node* node;
    SuperNode* dst;
    int gain;
    std::tie(node, dst, gain) = gainQueue.top();
    popGainQueue();
    if (gain <= 1) break;
    if (dst == node->super) continue;

    for (Node* prev : node->prev) { // previous node can become outcoming node or dst may change
      /* prev is no longer outcoming */
      if (prev->super == dst && outcomingMap.find(prev) != outcomingMap.end()) {
        outcomingMap.erase(prev);
        continue;
      }
      /* prev may become outcoming */
      if (node->super == prev->super) {
        SuperNode* outcoming = checkOutcoming(prev);
        if (outcoming) addOutcoming(prev, outcoming);
        continue;
      }
      if (outcomingMap.find(prev) != outcomingMap.end()) {
        SuperNode* outcoming = checkOutcoming(prev);
        if (outcoming) addOutcoming(prev, outcoming);
      }  
    }
  
    for (Node* next : node->next) {
      /* next is no longer incoming */
      if (next->super == dst && incomingMap.find(next) != incomingMap.end()) {
        incomingMap.erase(next);
        continue;
      }
      /* next may become incoming */
      if (node->super == next->super) {
        SuperNode* incoming = checkIncoming(next);
        if (incoming) addIncoming(next, incoming);
        continue;
      }
      /* update gain & dst */
      if (incomingMap.find(next) != incomingMap.end()) {
        SuperNode* incoming = checkIncoming(next);
        if (incoming) addIncoming(next, incoming);
      }
    }
    /* move node to dst */
    SuperNode* prevSuper = node->super;
    node->super = dst;
    for (Node* next : node->next) {
      if (next->super != dst) {
        anyExtEdge[node] = 1;
      }
    }
    for (Node* prev : node->prev) {
      if (prev->super == prevSuper) anyExtEdge[prev] = 1;
      else if(prev->super == dst) updateExtEdge(prev);
    }
  }
  /* construct super relationship */
  for (SuperNode* super : sortedSuper) super->member.clear();
  for (Node* node : allGainNodes) {
    node->super->member.push_back(node);
  }
  removeEmptySuper();
  reconnectSuper();
  printf("refine %d times\n", refineNum);
}

void graph::graphPartition() {
  size_t totalSuper = sortedSuper.size();
  size_t phaseSuper = sortedSuper.size();
  orderAllNodes();

/* coarsen phase */
  graphCoarsen();
  resort();
  printf("[graphCoarsen] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());

/* initial partition */
  phaseSuper = sortedSuper.size();
  graphInitPartition();
  orderAllNodes();
  printf("[InitPartition] remove %ld superNodes (%ld -> %ld)\n", phaseSuper - sortedSuper.size(), phaseSuper, sortedSuper.size());
/* refine & uncoarsen phase */
  // graphRefine();
  phaseSuper = sortedSuper.size();
  printf("[graphPartition] remove %ld superNodes (%ld -> %ld)\n", totalSuper - phaseSuper, totalSuper, phaseSuper);
}
