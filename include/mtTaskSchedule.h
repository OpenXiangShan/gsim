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

  SuperNode* super = nullptr;
  int id = -1;
  int chunkCount = 0;
  std::vector<Instruction> body;
};

// One pinned MTask per worker, including workers with an empty state body.
// Members own current-state storage / reset operations. The writer lists
// reference compute-phase producers of prior-cycle pending writes; those
// producers remain compute tasks and are NOT state-task members.
struct MtStateTask : MtTask {
  MtStateTask() { kind = MtTaskKind::StateUpdate; owner = -1; }
  int chunkCount = 0;
  size_t registerStorageBytes = 0;
  size_t memoryWriteBytes = 0;
  size_t sparseRegisterWriteBytes = 0;
  std::vector<Node*> registers;
  std::vector<Node*> memoryWriters;
  std::vector<Node*> sparseRegisterWriters;
  std::vector<MtReset::Instruction> body;
};

struct MtWorkerPlan {
  int readySlotCount_ = 1;
  std::vector<std::vector<int>> workerTasks_;
  std::vector<int> waitSlots_;
  std::vector<int> storeSlots_;
  std::vector<int> localWaitSlots_;
  // stateTasks_[w] -> all-worker state barrier -> workerTasks_[w].
  // State and compute task IDs have separate namespaces; no state task is
  // included in the token DAG, lookahead, or asynchronous-reset replay.
  std::vector<MtStateTask> stateTasks_;
  std::unordered_map<Node*, int> stateTaskByNode_;
  std::vector<MtReset> resets_;
};

class MtWorkerBuilder {
 public:
  static void build(graph& graph, MtTaskPlan& tasks, MtWorkerPlan& workers,
                    int workerCount, int resetChunk);
};

#endif
