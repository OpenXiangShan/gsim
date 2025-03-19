/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H

class graph {
  FILE *srcFp;
  int srcFileIdx;
  int srcFileBytes;

  bool __emitSrc(bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...);
  void emitPrintf();
  void activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName);
  void activateUncondNext(Node* node, std::set<int>activateId, bool inStep, std::string flagName);

  FILE* genHeaderStart();
  void genNodeDef(FILE* fp, Node* node);
  void genNodeInsts(Node* node, std::string flagName);
  void genInterfaceInput(Node* input);
  void genInterfaceOutput(Node* output);
  void genStep(int subStepIdxMax);
  void genHeaderEnd(FILE* fp);
  void genNodeStepStart(SuperNode* node, uint64_t mask, int idx, std::string flagName);
  void genNodeStepEnd(SuperNode* node);
  void genNodeInit(Node* node, int mode);
  void genMemInit(Node* node);
  void nodeDisplay(Node* member);
  void genMemRead(FILE* fp);
  int genActivate();
  void genUpdateRegister(FILE* fp);
  void genMemWrite(FILE* fp);
  void saveDiffRegs();
  void genResetAll();
  void genReset(SuperNode* super, bool isUIntReset);
  void genResetDecl(FILE* fp);
  std::string saveOldVal(Node* node);
  void removeNodesNoConnect(NodeStatus status);
  void reconnectSuper();
  void reconnectAll();
  void resetAnalysis();
  /* defined in mergeNodes */
  void mergeWhenNodes();
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
  std::vector<SuperNode*> uintReset;
  std::set<Node*> splittedArray;
  std::vector<std::string> extDecl;
  std::string name;
  int nodeNum = 0;
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
  void clockOptimize(std::map<std::string, Node*>& allSignals);
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
};

#endif
