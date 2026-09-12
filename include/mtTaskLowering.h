#ifndef MT_TASK_LOWERING_H
#define MT_TASK_LOWERING_H

#include "mtTaskPartition.h"

class graph;

class MtTaskLowerer {
 public:
  static void generateStmtTrees(graph& graph, MtTaskPlan& plan);
};

#endif
