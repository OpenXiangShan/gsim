#ifndef MT_TASK_PARTITION_H
#define MT_TASK_PARTITION_H

#include <cstdint>
#include <vector>

class graph;
class SuperNode;

struct MtTask {
  std::vector<int> cppIds;
  std::vector<int> predecessors;
  std::vector<int> successors;
  std::vector<int> waits;
  std::vector<int> stores;
  int cost = 0;
  int owner = 0;
  uint32_t waitBegin = 0;
  uint32_t waitEnd = 0;
  uint32_t storeBegin = 0;
  uint32_t storeEnd = 0;
};

struct MtTaskPlan {
  std::vector<SuperNode*> byCppId_;
  std::vector<int> topologicalCppIds_;
  std::vector<MtTask> tasks_;
};

class MtTaskPartitioner {
 public:
  static void build(graph& graph, MtTaskPlan& plan, int maxTasks);
};

#endif
