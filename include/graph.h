/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H
#include "common.h"

class graph {
  FILE* genHeaderStart();
  FILE* genSrcStart();
  void genNodeDef(FILE* fp, Node* node);
  void genNodeInsts(FILE* fp, Node* node, std::string flagName);
  void genInterfaceInput(FILE* fp, Node* input);
  void genInterfaceOutput(FILE* fp, Node* output);
  void genThreadStep(FILE* fp, int threadId);
  void genStep(FILE* fp);
  void genHeaderEnd(FILE* fp);
  void genSrcEnd(FILE* fp);
  void genNodeStepStart(FILE* fp, SuperNode* node, uint64_t mask, int idx, std::string flagName);
  void genNodeStepEnd(FILE* fp, SuperNode* node);
  void genNodeInit(FILE* fp, Node* node);
  void genMemInit(FILE* fp, Node* node);
  void nodeDisplay(FILE* fp, Node* member);
  void genMemRead(FILE* fp);
  void genActivate(FILE* fp, int threadId);
  void genUpdateRegister(FILE* fp);
  void genMemWrite(FILE* fp);
  void saveDiffRegs(FILE* fp);
  void genResetAll(FILE* fp);
  void genReset(FILE* fp, SuperNode* super, bool isUIntReset);
  void genResetDecl(FILE* fp);
  void genSyncClassDef(FILE* fp);
  void genSyncDef(FILE* fp);
  void computeSync();
  std::string saveOldVal(FILE* fp, Node* node);
  void removeNodesNoConnect(NodeStatus status);
  void reconnectSuper();
  void reconnectAll();
  void resetAnalysis();
  /* defined in mergeNodes */
  void mergeAsyncReset();
  void mergeUIntReset();
  void mergeOut1();
  void mergeIn1();
  void mergeSublings();
  void mergeNear();
  void splitArrayNode(Node* node);
  void checkNodeSplit(Node* node);
  void splitOptionalArray();
  void constantMemory();
  void orderAllNodes();
  void genDiffSig(FILE* fp, Node* node);
  void removeDeadReg();
  void graphCoarsen();
  void graphInitPartition();
  void graphRefine();
  void resort();
  void detectSortedSuperLoop();
  void distrSuperThread(SuperNode* super, int threadId);
 public:
  std::vector<Node*> allNodes;
  std::vector<Node*> input;
  std::vector<Node*> output;
  std::vector<Node*> regsrc;
  std::vector<Node*> sorted;
  std::vector<Node*> memory;
  std::vector<Node*> external;
  std::set<Node*> halfConstantArray;
  std::vector<Node*> specialNodes;
  /* used before toposort */
  std::vector<SuperNode*> supersrc;
  /* used after toposort */
  std::vector<SuperNode*> sortedSuper;
  std::vector<std::vector<SuperNode*>> uintReset;
  std::set<Node*> splittedArray;
  std::vector<std::string> extDecl;
  std::string name;
/* parallel */
  std::vector<std::vector<SuperNode*>> parallelSuper;
  int maxTmp = 0;
  int nodeNum = 0;
  int subStepNum[THREAD_NUM];
  int updateRegNum = -1;
  void addReg(Node* reg) {
    regsrc.push_back(reg);
  }
  void detectLoop();
  void topoSort();
  void instsGenerator();
  void cppEmitter();
  void usedBits();
  void traversal();
  void traversalNoTree();
  void splitArray();
  void removeDeadNodes();
  void aliasAnalysis();
  void mergeNodes();
  size_t countNodes();
  void removeEmptySuper();
  void removeNodes(NodeStatus status);
  void mergeRegister();
  void clockOptimize();
  void constantAnalysis();
  void constructRegs();
  void commonExpr();
  void splitNodes();
  void replicationOpt();
  void perfAnalysis();
  void exprOpt();
  void patternDetect();
  void graphPartition();
  void MFFCPartition();
  void mergeEssentSmallSubling(size_t maxSize, double sim);
  void essentPartition();
  void inferAllWidth();
  void dump(std::string FileName);
  void depthPerf();
  void parallelPartition();
  graph() {
    parallelSuper = std::vector<std::vector<SuperNode*>>(THREAD_NUM);
    uintReset = std::vector<std::vector<SuperNode*>>(THREAD_NUM);
  }
};

#endif
