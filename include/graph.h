/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H

struct SourceAggregate {
  int id;
  std::string name;
  std::vector<int> memberIds;
  std::vector<int> originalMemberIds;
  std::vector<bool> memberFlips;
  std::vector<int> nestedAggregateIds;
  std::vector<std::string> semanticPath;
  int semanticScopeDepth = 0;
};

struct SourceReplacement {
  int originalId;
  int replacementId;
  std::string originalName;
  int originalWidth;
  bool originalSign;
  std::vector<int> originalDimension;
  int originalDepth;
};

struct SourceScope {
  std::string path;
  std::string module;
  int line;
  int instanceLine;
  bool external;
  std::vector<std::string> semanticPath;
};

class graph {
  FILE *srcFp;
  int srcFileIdx;
  int srcFileBytes;

  bool __emitSrc(int indent, bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...);
  void emitPrintf();
  void activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName, int indent);
  void activateUncondNext(Node* node, std::set<int>& activateId, bool inStep, std::string flagName, int indent);

  FILE* genHeaderStart();
  void genNodeDef(FILE* fp, Node* node);
  void genInterfaceInput(Node* input);
  void genInterfaceOutput(Node* output);
  void genStep(int subStepIdxMax);
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
  void splitArrayNode(Node* node, const char* reason);
  void checkNodeSplit(Node* node);
  void splitOptionalArray();
  void constantMemory();
  void orderAllNodes();
  void genDiffSig(FILE* fp, Node* node);
  void graphCoarsen();
  void graphInitPartition();
  void graphRefine();
  void resort();
  void detectSortedSuperLoop();
  void when2mux();
 public:
  bool sourceAggregateInventoryComplete = false;
  std::vector<SourceAggregate> sourceAggregates;
  bool sourceScopeInventoryComplete = false;
  std::vector<SourceScope> sourceScopes;
  bool sourceReplacementInventoryComplete = false;
  std::vector<SourceReplacement> sourceReplacements;
  const std::map<Node*, std::vector<Node*>>& splitArraySources() const;
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
  void MFFCPartition();
  void mergeEssentSmallSubling(size_t maxSize, double sim);
  void essentPartition();
  void inferAllWidth();
  void dump(std::string FileName);
  bool dumpStage(std::string stageName);
  void depthPerf();
  void generateStmtTree();
  void connectDep();
};

#endif
