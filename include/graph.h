/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H

class CppEmitterMt;
class MtTaskPartitioner;
class MtWorkerBuilder;

class graph {
  friend class CppEmitterMt;
  friend class MtTaskPartitioner;
  friend class MtWorkerBuilder;
  FILE *srcFp;
  int srcFileIdx;
  int srcFileBytes;

  bool __emitSrc(int indent, bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...);
  bool __emitSrcMt(int indent, bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...);
  void emitPrintf();
  void emitPrintfMt();
  void activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName, int indent,
                    std::string newName = "");
  void activateUncondNext(Node* node, std::set<int>& activateId, bool inStep, std::string flagName, int indent);

  FILE* genHeaderStart();
  FILE* genHeaderStartMt();
  void genNodeDef(FILE* fp, Node* node);
  void genNodeDefMt(FILE* fp, Node* node, bool taskLocal = false,
                    const std::string& emittedName = "");
  void genInterfaceInput(Node* input);
  void genInterfaceInputMt(Node* input);
  void genInterfaceOutput(Node* output);
  void genInterfaceOutputMt(Node* output);
  void genStep(int subStepIdxMax);
  void emitActiveMask(std::set<int>& activeId, int indent);
  void emitRepCutFns(const std::vector<SuperNode*>& supers, const std::string& prefix, int& chunks);
  void genRepCutEval(int part);
  void genRepCutCommit();
  void genRepCutStep();
  void genRepCutResetRefresh();
  void genRepCutMemoryDefer();
  void genHeaderEnd(FILE* fp);
  int genNodeStepStart(SuperNode* node, uint64_t mask, int idx, std::string flagName, int indent);
  int genNodeStepEnd(SuperNode* node, int indent);
  void genMemInit(Node* node);
  void nodeDisplay(Node* member, int indent);
  void genMemRead(FILE* fp);
  int genActivate();
  void genUpdateRegister(FILE* fp);
  void genMemWrite(FILE* fp);
  void saveDiffRegs();
  void genResetAll();
  void genResetDef(SuperNode* super, bool isUIntReset, int indent);
  void genResetActivation(SuperNode* super, bool isUIntReset, int indent, int resetId);
  void genResetDecl(FILE* fp);
  int translateInst(InstInfo inst, int indent, std::string flagName);
  void genSuperEval(SuperNode* super, std::string flagName, int indent);
  void removeNodesNoConnect(NodeStatus status);
  void reconnectSuper();
  void reconnectAll();
  void resetAnalysis();
  /* graphPartition */
  void mergeWhenNodes();
  void mergeResetAll();
  void mergeOut1();
  void mergeIn1();
  void mergeSublings();
  void splitArrayNode(Node* node);
  void splitArrayNode(Node* node, const std::vector<int>& splitDims);
  void checkNodeSplit(Node* node);
  void splitOptionalArray();
  void splitConstantAxes();
  void constantMemory();
  void orderAllNodes();
  void genDiffSig(FILE* fp, Node* node);
  void genDiffSigMt(FILE* fp, Node* node, const std::string& emittedName = "");
  void graphCoarsen(bool withReset = true);
  void graphInitPartition();
  void graphRefine();
  void resort();
  void detectSortedSuperLoop();
  void when2mux();
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
  std::vector<SuperNode*> allReset;
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
  void cppEmitterMt();
  void usedBits();
  void traversal();
  void traversalNoTree();
  void splitArray();
  void removeDeadNodes();
  void aliasAnalysis();
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
  void partitionSorted(bool withReset);
  void repCutLower();
  void repCutSplit();
  void finalizeRepCutParts();
  void MFFCPartition();
  void mergeEssentSmallSubling(size_t maxSize, double sim);
  void essentPartition();
  void inferAllWidth();
  void dump(std::string FileName);
  void depthPerf();
  void generateStmtTree();
  void connectDep();
};

int repCutPartOf(SuperNode* super);
bool repCutCommitOnly(SuperNode* super);
bool repCutMergeOk(SuperNode* a, SuperNode* b);
bool repCutSnapshotMemory(Node* memory);
int repCutMemoryLogPart(Node* access);
const std::vector<int>& repCutSnapshotWriterParts(Node* memory);
const std::vector<Node*>& repCutSnapshotMemories();
void repCutNoteBoundary(Node* from, Node* to);
const std::vector<Node*>& repCutBoundaryNext(Node* node);
void repCutPruneEdges(const std::vector<SuperNode*>& supers);
bool repCutLocalGraph();

#endif
