#ifndef MT_REPLICATION_SCHEDULE_H
#define MT_REPLICATION_SCHEDULE_H

#include "mtTaskPartition.h"
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// A consumer-specific, backward cone. Nodes are in dependency-first order.
// Boundary nodes are reused, never cloned or charged against the copy budget.
struct MtReplicationCone {
  std::vector<Node*> nodes;
  std::vector<Node*> sourceInputs;
  std::vector<Node*> boundaryInputs;
  int operations = 0;
  std::string rejection;
  bool valid() const { return rejection.empty(); }
};

// The caller supplies time/worker-specific availability. Reused remote values
// remain explicit boundaries and still require synchronization in the plan.
MtReplicationCone mtBuildAvailableCone(
    const std::vector<Node*>& roots, int maxOps,
    const std::function<bool(Node*)>& available);


struct MtScheduledInterval {
  int task = -1;
  long long start = 0;
  long long finish = 0;
};

struct MtHeftPlacement {
  int task = -1, worker = -1;
  long long start = 0, finish = 0;
  long long waitStart = 0, waitFinish = 0;
  long long copyStart = -1, copyFinish = -1, extraWork = 0;
  int cost = 0, copyCost = 0;
  MtReplicationCone cone;
  std::vector<int> dependencies, copyDependencies;
  std::unordered_map<Node*, Node*> redirects;
};

// Called inside the ordinary HEFT worker loop, after original tasks are
// lowered. Candidate evaluation never mutates the scheduled graph.
class MtHeftReplication {
 public:
  MtHeftReplication(graph& graph, MtTaskPlan& tasks, int workers);
  ~MtHeftReplication();
  MtHeftPlacement evaluate(MtHeftPlacement native,
      const std::vector<int>& owners, const std::vector<long long>& completion,
      const std::map<long long, MtScheduledInterval>& timeline);
  int commit(const MtHeftPlacement& placement);
  void finish();
  void report(const std::vector<int>& newIds, long long makespan) const;
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

#endif
