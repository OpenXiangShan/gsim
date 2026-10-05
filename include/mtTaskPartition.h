#ifndef MT_TASK_PARTITION_H
#define MT_TASK_PARTITION_H

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class graph;
class ExpTree;
class InstInfo;
class Node;
class StmtTree;

enum class MtTaskKind : uint8_t {
  Normal,
  ExtModule,
  StateUpdate,
};

// The value is published by the cycle-start state barrier, not by an MTask
// token. Keep this predicate shared by partitioning, lowering and scheduling.
bool mtIsCycleStartRegisterUpdate(const Node* node);
// Structural work units for partitioning; worker scheduling uses lowered code.
int mtStructuralNodeCost(const Node* node);

struct MtTask {
  std::vector<Node*> members;
  std::vector<int> predecessors;
  std::vector<int> successors;
  std::vector<int> waits;
  std::vector<int> stores;
  std::vector<int> localWaits;
  std::vector<InstInfo>* insts = nullptr;
  StmtTree* stmtTree = nullptr;
  std::unordered_set<Node*> localNodes;
  MtTaskKind kind = MtTaskKind::Normal;
  int globalNodeCount = 0;
  int cost = 0;
  int owner = 0;
  uint32_t waitBegin = 0;
  uint32_t waitEnd = 0;
  uint32_t storeBegin = 0;
  uint32_t storeEnd = 0;
  uint32_t localWaitBegin = 0;
  uint32_t localWaitEnd = 0;
};

// A large register array whose next-state updates are represented as ordered
// address/data/valid writes instead of a second full-size array.
struct MtSparseRegister {
  Node* source = nullptr;
  std::vector<Node*> writers;
  std::vector<ExpTree*> writeTrees;
  std::size_t storageBytes = 0;
};

struct MtTaskPlan {
  // Only compute-phase tasks are partitioned and scheduled by HEFT/list.
  std::vector<MtTask> tasks_;
  std::unordered_map<Node*, int> taskByNode_;
  // Removed before coarsening; assigned to per-worker state tasks later.
  std::vector<Node*> stateRegisters_;
  std::unordered_map<Node*, int> partitionGroupByNode_;
  std::vector<Node*> emissionNodes_;
  std::unordered_set<Node*> localNodes_;
  std::unordered_map<Node*, int> workerLocalOwners_;
  std::vector<MtSparseRegister> sparseRegisters_;
  std::unordered_map<Node*, std::size_t> sparseRegisterBySource_;
  std::unordered_map<Node*, std::size_t> sparseRegisterByDestination_;
  int partitionGroupCount_ = 0;
};

class MtTaskPartitioner {
 public:
  static void build(graph& graph, MtTaskPlan& plan, int targetTasks);
  static void useDirectDependencies(graph& graph);
};

#endif
