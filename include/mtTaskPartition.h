#ifndef MT_TASK_PARTITION_H
#define MT_TASK_PARTITION_H

#include <cstdint>
#include <unordered_set>
#include <vector>

class graph;
class InstInfo;
class Node;
class SuperNode;
class StmtTree;

struct MtTask {
  std::vector<int> cppIds;
  std::vector<Node*> members;
  std::vector<int> predecessors;
  std::vector<int> successors;
  std::vector<int> waits;
  std::vector<int> stores;
  std::vector<InstInfo>* insts = nullptr;
  StmtTree* stmtTree = nullptr;
  std::unordered_set<Node*> localNodes;
  int globalNodeCount = 0;
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
  std::vector<int> taskByCppId_;
  std::vector<MtTask> tasks_;
  std::unordered_set<Node*> localNodes_;
};

class MtTaskPartitioner {
 public:
  static void assignCppIds(graph& graph);
  static void build(graph& graph, MtTaskPlan& plan, int maxTasks);
};

#endif
