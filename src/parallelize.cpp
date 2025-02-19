#include "common.h"

void graph::distrSuperThread(SuperNode* super, int threadId) {
  parallelSuper[threadId].push_back(super);
  super->threadId = threadId;
}

void graph::parallelPartition() {
  for (SuperNode* super : sortedSuper) Assert(super->member.size() == 1, "super %d has %ld members\n", super->id, super->member.size());
  std::vector<int> nodeNum(THREAD_NUM, 0);
  int totalSource = 0;
  for (SuperNode* super : sortedSuper) {
    if (super->prev.size() == 0) totalSource ++;
  }
  int sourcePerThread = totalSource / THREAD_NUM;
  int curThread = 0;
  size_t curSourceNum = 0;
  for (SuperNode* super : sortedSuper) {
    int validPrev = 0;
    for (SuperNode* prev : super->prev) {
      if (prev->superType == SUPER_VALID) validPrev ++;
    }
    Node* node = super->member[0];
    if (validPrev == 0) {  // source node
      distrSuperThread(super, curThread);
      curSourceNum ++;
      if (curSourceNum >= sourcePerThread && curThread < THREAD_NUM - 1) {
        curThread ++;
        curSourceNum = 0;
      }
    } else if (node->type == NODE_REG_DST) { // register dst
      distrSuperThread(super, node->getSrc()->super->threadId);
    } else { // intermediate node
      std::vector<int> prevThread(THREAD_NUM, 0);
      for (SuperNode* prev : super->prev) {
        if (prev->superType != SUPER_VALID) continue;
        Assert(prev->threadId >= 0 && prev->threadId < THREAD_NUM, "invalid threadId %d (super %d <- prev %d)\n", prev->threadId, super->id, prev->id);
        prevThread[prev->threadId] ++;
      }
      /* find max */
      int selectThread = 0;
      for (int i = 1; i < THREAD_NUM; i ++) {
        if (prevThread[i] > prevThread[selectThread]) selectThread = i;
        if (prevThread[i] == prevThread[selectThread] && nodeNum[i] < nodeNum[selectThread]) selectThread = i;
      }
      distrSuperThread(super, selectThread);
    }
  }
}