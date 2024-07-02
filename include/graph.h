/*
  graph class which describe the whole design graph
*/

#ifndef GRAPH_H
#define GRAPH_H

class graph {
  FILE* genHeaderStart(std::string headerFile);
  FILE* genSrcStart(std::string name);
  void genNodeDef(FILE* fp, Node* node);
  void genNodeInsts(FILE* fp, Node* node);
  void genInterfaceInput(FILE* fp, Node* input);
  void genInterfaceOutput(FILE* fp, Node* output);
  void genStep(FILE* fp);
  void genHeaderEnd(FILE* fp);
  void genSrcEnd(FILE* fp);
  void genNodeStepStart(FILE* fp, SuperNode* node);
  void genNodeStepEnd(FILE* fp, SuperNode* node);
  void genNodeInit(FILE* fp, Node* node);
  void genMemInit(FILE* fp, Node* node);
  void nodeDisplay(FILE* fp, SuperNode* super);
  void genMemRead(FILE* fp);
  void genActivate(FILE* fp);
  void genUpdateRegister(FILE* fp);
  void genMemWrite(FILE* fp);
  void saveDiffRegs(FILE* fp);
  void genReset(FILE* fp);
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
  void splitArrayNode(Node* node);
  void checkNodeSplit(Node* node);
  void splitOptionalArray();
  void constantMemory();
  void orderAllNodes();
  void genDiffSig(FILE* fp, Node* node);
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
  std::set<SuperNode*> supersrc;
  /* used after toposort */
  std::vector<SuperNode*> sortedSuper;
  std::vector<SuperNode*> uintReset;
  std::set<Node*> splittedArray;
  std::string name;
  int maxTmp = 0;
  int nodeNum = 0;
  int subStepNum = -1;
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
  bool inSrc(SuperNode* super);
  void mergeRegister();
  void clockOptimize();
  void constantAnalysis();
  void constructRegs();
  void commonExpr();
  void splitNodes();
  void exprOpt();
};

#endif
