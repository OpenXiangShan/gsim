#include "common.h"

Config::Config() {}
Config globalConfig;

int main(int argc, char** argv) {
  Assert(argc == 2, "expected output directory");
  globalConfig.OutputDir = argv[1];
  globalConfig.DumpGraphDot = globalConfig.DumpGraphJson = globalConfig.DumpGraphStats = false;
  globalConfig.DumpExpressions = true;
  globalConfig.ExpressionKinds = {"when-condition", "memory-read-address"};
  globalConfig.ExpressionClosure = "combinational";
  globalConfig.ExpressionMaxNodes = 100;
  graph circuit;
  circuit.name = "SyntheticExpressions";
  Node input(NODE_INP), condition, left, right, source(NODE_REG_SRC), destination(NODE_REG_DST);
  Node memory(NODE_MEMORY), reader(NODE_READER), intermediate;
  input.name = "input";
  condition.name = "condition";
  left.name = "left";
  right.name = "right";
  source.name = "address";
  destination.name = "addressNext";
  memory.name = "memory";
  reader.name = "reader";
  intermediate.name = "intermediate";
  intermediate.assignTree.push_back(new ExpTree(new ENode(&input), &intermediate));
  memory.set_memory(4, 1, 1);
  memory.extraInfo = "undefined";
  memory.add_member(&reader);
  condition.sourceKind = Node::SourceKind::WhenCondition;
  condition.sourceLine = 17;
  condition.assignTree = {nullptr, new ExpTree(new ENode(&input), &condition)};
  ENode* whenTrue = new ENode(OP_WHEN);
  whenTrue->child = {new ENode(&condition), new ENode(OP_INVALID), nullptr};
  left.assignTree.push_back(new ExpTree(whenTrue, &left));
  static_assert(OP_STMT_NODE < 63);
  ENode* unknown = new ENode(static_cast<OPType>(63));
  unknown->strVal = "raw\nunknown\"payload";
  unknown->values = {3, -2};
  ENode* whenFalse = new ENode(OP_WHEN);
  whenFalse->child = {new ENode(&condition), nullptr, unknown};
  right.assignTree.push_back(new ExpTree(whenFalse, &right));
  source.sourceKind = Node::SourceKind::MemoryReadAddress;
  source.sourceLine = 20;
  source.sourceOwnerId = reader.id;
  source.bindReg(&destination);
  source.clock = destination.clock = &input;
  source.resetCond = new ExpTree(new ENode(&input), &source);
  source.resetVal = new ExpTree(new ENode(OP_INVALID), &source);
  source.resetTree = new ExpTree(new ENode(OP_RESET), &source);
  destination.assignTree.push_back(new ExpTree(new ENode(&intermediate), &destination));
  ENode* read = new ENode(OP_READ_MEM);
  read->memoryNode = &memory;
  read->child = {new ENode(&source), nullptr};
  reader.memTree = new ExpTree(read, &reader);
  reader.clock = &input;
  reader.valTree = new ExpTree(new ENode());
  reader.valTree->setRoot(nullptr);
  circuit.allNodes = {&input, &condition, &left, &right, &source, &destination, &reader, &intermediate};
  circuit.memory = {&memory};
  circuit.dump("Synthetic");
  globalConfig.ExpressionClosure = "source-cuts";
  circuit.dump("SourceCuts");
  globalConfig.ExpressionClosure = "combinational";
  globalConfig.ExpressionMaxNodes = 1;
  circuit.dump("Capped");
  globalConfig.ExpressionMaxNodes = 100;
  globalConfig.ExpressionKinds.clear();
  globalConfig.ExpressionIdPath = globalConfig.OutputDir + "/seed-ids.txt";
  {
    std::ofstream ids(globalConfig.ExpressionIdPath);
    ids << condition.id << '\n';
  }
  circuit.dump("IdFiltered");
  {
    std::ofstream ids(globalConfig.ExpressionIdPath);
    ids << 999999999 << '\n';
  }
  circuit.dump("MissingSeed");
  return 0;
}
