#ifndef MT_TASK_SCHEDULE_H
#define MT_TASK_SCHEDULE_H

#include "mtTaskPartition.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class graph;
class Node;
class SuperNode;

struct MtReset {
  struct Instruction {
    uint8_t type = 0;
    std::string text;
  };

  struct Worker {
    int id = -1;
    int chunkCount = 0;
    std::vector<Instruction> body;
  };

  SuperNode* super = nullptr;
  int id = -1;
  bool asynchronous = false;
  int chunkCount = 0;
  int triggerTask = -1;
  int triggerOwner = -1;
  std::vector<Instruction> body;
  std::vector<Worker> workers;
  std::vector<int> participants;
};

struct MtResetJoin {
  int resetId = -1;
  size_t beforePosition = 0;
};

struct MtWorkerPlan {
  int readySlotCount_ = 1;
  std::vector<std::vector<int>> workerTasks_;
  std::vector<int> waitSlots_;
  std::vector<int> storeSlots_;
  std::vector<MtReset> resets_;
  std::vector<std::vector<MtResetJoin>> resetJoins_;
  std::map<Node*, int> asyncResetIds_;
};

class MtWorkerBuilder {
 public:
  static void build(graph& graph, MtTaskPlan& tasks, MtWorkerPlan& workers,
                    int workerCount, int resetChunk);
};

#endif
