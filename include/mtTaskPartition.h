#ifndef MT_TASK_PARTITION_H
#define MT_TASK_PARTITION_H

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class graph;
class InstInfo;
class Node;
class StmtTree;

enum class MtTaskKind : uint8_t {
  Normal,
  ExtModule,
  AsyncReset,
};

struct MtTask {
  std::vector<Node*> members;
  std::vector<int> predecessors;
  std::vector<int> successors;
  std::vector<int> waits;
  std::vector<int> stores;
  std::vector<InstInfo>* insts = nullptr;
  StmtTree* stmtTree = nullptr;
  std::unordered_set<Node*> localNodes;
  MtTaskKind kind = MtTaskKind::Normal;
  Node* resetNode = nullptr;
  int globalNodeCount = 0;
  int cost = 0;
  int owner = 0;
  uint32_t waitBegin = 0;
  uint32_t waitEnd = 0;
  uint32_t storeBegin = 0;
  uint32_t storeEnd = 0;
};

struct MtTaskPlan {
  std::vector<MtTask> tasks_;
  std::unordered_map<Node*, int> taskByNode_;
  std::unordered_map<Node*, int> partitionGroupByNode_;
  std::vector<Node*> emissionNodes_;
  std::unordered_set<Node*> localNodes_;
  std::unordered_map<Node*, int> workerLocalOwners_;
  int partitionGroupCount_ = 0;
};

class MtTaskPartitioner {
 public:
  static void build(graph& graph, MtTaskPlan& plan, int targetTasks);
};

#endif
