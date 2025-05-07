/*
  nodes in circuit graph
*/

#ifndef NODE_H
#define NODE_H

#include "debug.h"

class NodeComponent;
class StmtTree;

enum NodeType{
  NODE_INVALID,
  NODE_REG_SRC,
  NODE_REG_DST,
  NODE_SPECIAL, // printf & assert
  NODE_INP,
  NODE_OUT,
  NODE_MEMORY,
  NODE_READER,
  NODE_WRITER,
  NODE_READWRITER,
  NODE_INFER,
  NODE_MEM_MEMBER,
  NODE_OTHERS,
  NODE_REG_UPDATE,
  NODE_EXT_IN,
  NODE_EXT_OUT,
  NODE_EXT,
};

enum NodeStatus{ VALID_NODE, DEAD_NODE, CONSTANT_NODE, MERGED_NODE, DEAD_SRC, REPLICATION_NODE, SPLITTED_NODE };
enum IndexType{ INDEX_INT, INDEX_NODE };
enum AsReset { EMPTY, NODE_ASYNC_RESET, NODE_UINT_RESET, NODE_ALL_RESET};

enum ReaderMember { READER_ADDR = 0, READER_EN, READER_CLK, READER_DATA, READER_MEMBER_NUM};
enum WriterMember { WRITER_ADDR = 0, WRITER_EN, WRITER_CLK, WRITER_DATA, WRITER_MASK, WRITER_MEMBER_NUM};
enum ReadWriterMember { READWRITER_ADDR = 0, READWRITER_EN, READWRITER_CLK,
                      READWRITER_RDATA, READWRITER_WDATA, READWRITER_WMASK,
                      READWRITER_WMODE, READWRITER_MEMBER_NUM};

class TypeInfo {
public:
  int width;
  bool sign;
  bool clock = false;
  ResetType reset = UNCERTAIN;
  std::vector<int> dimension;
  std::vector<std::pair<Node*, bool>> aggrMember; // all members, if not empty, all other infos above are invalid
  std::vector<AggrParentNode*> aggrParent; // the later the outer

  void add(Node* node, bool isFlip) { aggrMember.push_back(std::make_pair(node, isFlip)); }
  void set_width(int _width) { width = _width; }
  void set_sign(bool _sign) { sign = _sign; }
  void set_clock(bool is_clock) { clock = is_clock; }
  bool isClock() { return clock; }
  void set_reset(ResetType type) { reset = type; }
  ResetType getReset() { return reset; }
  void mergeInto(TypeInfo* info);
  bool isAggr() { return aggrParent.size() != 0; }
  void addDim(int num);
  void newParent(std::string name);
  void flip();
};

/*
e.g. {{a, b}x, {c}y }z;
  z.parent = {z_x, z_y} z.member = {z_x_a, z_x_b, z_y_c};
  x.parent = {}, x.member = {z_x_a, z_x_b}
*/
class AggrParentNode {  // virtual type_aggregate node, used for aggregate connect
  public:
  std::string name;
  std::vector<std::pair<Node*, bool>> member;  // leaf member
  std::vector<AggrParentNode*> parent; // non-leaf member (aggregate type), increasing partial order
  AggrParentNode(std::string _name, TypeInfo* info = nullptr);
  int size() {
    return member.size();
  }
  void addMember(Node* _member, bool isFlip) {
    member.push_back(std::make_pair(_member, isFlip));
  }
  void addParent(AggrParentNode* _parent) {
    parent.push_back(_parent);
  }
};

class ExprValue {
public:
  std::vector<PNode*> ops;
  std::vector<Node*> operands;
  std::string consVal;
};

class Node {
  void finalConnect(std::string lvalue, valInfo* info);
  static int counter;
 public:

  Node(NodeType _type = NODE_OTHERS) {
    type    = _type;
    id = counter ++;
  }

  std::string name;  // concat the module name in order (member in structure / temp variable)
  std::string extraInfo; // ruw for memory
  int id = -1;
  NodeType type;
  int width = -1;
  bool sign = false;
  int usedBit = -1;
  NodeStatus status = VALID_NODE;
  std::vector<int> dimension;
  int order = -1;
  int orderInSuper = -1;
  int ops = 0;
  int lineno = -1;
  /* adjacent */
  std::set<Node*> next;
  std::set<Node*> prev;
  /* dependent but not adjacent
   * e.g. reg_src -> node1; node2->reg_dst; then:
   * node1 is depPrev of reg_dst, as activeFlags of node1 must first be cleared before reg_dst is activated
  */
  std::set<Node*> depPrev;
  std::set<Node*> depNext;
  std::vector <ExpTree*> assignTree;
  ExpTree* valTree = nullptr;
  ExpTree* memTree = nullptr;
  ExpTree* resetCond = nullptr;  // valid in reg_src
  ExpTree* resetVal = nullptr;   // valid in reg_src, used in AST2Graph
  std::set<int> invalidIdx;

  SuperNode* super = nullptr;
/* used for memory */
  int rlatency;
  int wlatency;
  int depth;
  std::vector<Node*> member;
  Node* parent = nullptr;
/* used for registers */
  Node* regNext = nullptr;
  Node* regUpdate = nullptr;
  ExpTree* updateTree = nullptr;
  ExpTree* resetTree = nullptr;
  bool regSplit = true;
/* used for instGerator */
  valInfo* computeInfo = nullptr;
/* used for splitted array */
  std::vector<Node*>arrayMember;
  int arrayIdx = -1;
  Node* arrayParent = nullptr;
/* used for reg & memory */
  Node* clock = nullptr;
  bool isClock = false;
  ResetType reset = UNCERTAIN;
  AsReset asReset = EMPTY;
  bool isArrayMember = false;
  bool inAggr = false;
/* used for visitWhen in AST2Graph */

  size_t whenDepth = 0;

/* used in instsGenerator */
  bool fullyUpdated = true;
  bool nodeIsRoot = false;

/* used in cppEmitter */
  std::set<int> nextActiveId;
  std::set<int> nextNeedActivate;

  std::vector<std::string> insts;
  std::vector<std::string> resetInsts;
  std::vector<std::string> initInsts;

  void updateInfo(TypeInfo* info);
  void setType(int _width, bool _sign) {
    width = _width;
    sign = _sign;
  }
  Node* dup(NodeType _type = NODE_INVALID, std::string _name = "");
  /* bind two the splitted registers */
  void bindReg(Node* reg) {
    regNext = reg;
    reg->regNext = this;
  }

  Node* getDst () {
    Assert(type == NODE_REG_SRC || type == NODE_REG_DST, "The node %s is not register", name.c_str());
    if (type == NODE_REG_SRC) return this->regNext;
    return this;
  }
  Node* getSrc () {
    Assert(type == NODE_REG_SRC || type == NODE_REG_DST, "The node %s is not register", name.c_str());
    if (type == NODE_REG_DST) return this->regNext;
    return this;
  }
  Node* getBindReg() {
    Assert(type == NODE_REG_SRC || type == NODE_REG_DST, "The node %s is not register", name.c_str());
    return this->regNext;
  }
  void set_memory(int _depth, int r, int w) {
    depth = _depth;
    rlatency = r;
    wlatency = w;
  }
  void add_member(Node* _member) {
    member.push_back(_member);
    if(_member) _member->set_parent(this);
  }
  void set_member(int idx, Node* node) {
    Assert(idx < (int)member.size(), "idx %d is out of bound [0, %ld)", idx, member.size());
    member[idx] = node;
    node->set_parent(this);
  }
  Node* get_member(int idx) {
    Assert(idx < (int)member.size(), "idx %d is out of bound [0, %ld)", idx, member.size());
    return member[idx];
  }
  void set_writer() {
    if (type == NODE_READWRITER) return;
    Assert(type == NODE_WRITER || type == NODE_INFER, "invalid type %d\n", type);
    type = NODE_WRITER;
  }
  void set_reader() {
    Assert(type == NODE_READER || type == NODE_INFER, "invalid type %d\n", type);
    type = NODE_READER;
  }
  Node* get_port_clock() {
    Assert(type == NODE_READER || type == NODE_WRITER, "invalid type %d in node %s", type, name.c_str());
    if (type == NODE_READER) return get_member(READER_CLK);
    else if (type == NODE_WRITER) return get_member(WRITER_CLK);
    Panic();
  }
  void set_parent(Node* _parent) {
    Assert(!parent, "parent in %s is already set", name.c_str());
    parent = _parent;
  }
  bool isArray() {
    return dimension.size() != 0;
  }
  bool arraySplitted() {
    return arrayMember.size() != 0;
  }
  void addArrayVal(ExpTree* val);
  Node* getArrayMember(int idx) {
    Assert(idx < (int)arrayMember.size(), "idx %d out of bound [0, %ld) in %s", idx, arrayMember.size(), name.c_str());
    return arrayMember[idx];
  }
  void set_super(SuperNode* _super) {
    super = _super;
  }
  void update_usedBit(int bits) {
    usedBit = MIN(width, MAX(bits, usedBit));
  }
  void setAsyncReset() {
    if (asReset == NODE_UINT_RESET || asReset == NODE_ALL_RESET) asReset = NODE_ALL_RESET;
    else asReset = NODE_ASYNC_RESET;
  }
  void setUIntReset() {
    if (asReset == NODE_ASYNC_RESET || asReset == NODE_ALL_RESET) asReset = NODE_ALL_RESET;
    else asReset = NODE_UINT_RESET;
  }
  bool isAsyncReset() {
    return asReset == NODE_ASYNC_RESET || asReset == NODE_ALL_RESET;
  }
  bool isUIntReset() {
    return asReset == NODE_UINT_RESET || asReset == NODE_ALL_RESET;
  }
  bool isReset() {
    return asReset == NODE_UINT_RESET || asReset == NODE_ASYNC_RESET || asReset == NODE_ALL_RESET;
  }
  bool isExt() {
    return type == NODE_EXT || type == NODE_EXT_IN || type == NODE_EXT_OUT;
  }
  void clear_relation();
  void addPrev(Node* node);
  void addPrev(std::set<Node*>& super);
  void addPrev(std::vector<Node*>& super);
  void erasePrev(Node* node);
  void addNext(Node* node);
  void addNext(std::set<Node*>& super);
  void addNext(std::vector<Node*>& super);
  void eraseNext(Node* node);
  void clearPrev();
  void updateDep();
  void updateConnect();
  void inferWidth();
  void clearWidth();
  void addReset();
  void addUpdateTree();
  void constructSuperNode(); // alloc superNode for every node
  void constructSuperConnect(); // connect superNode
  valInfo* compute(); // compute node
  valInfo* computeArray();
  valInfo* computeConstantArray();
  void recompute();
  void recomputeConstant();
  void passWidthToPrev();
  void splitArray();
  Node* arrayMemberNode(int idx);
  ENode* isAlias();
  bool anyExtEdge();
  bool needActivate();
  bool anyNextActive();
  void updateActivate();
  void updateNeedActivate(std::set<int>& alwaysActive);
  void removeConnection();
  void allocArrayVal();
  Node* clockAlias();
  clockVal* clockCompute();
  ResetType inferReset();
  void setConstantZero(int w = -1);
  void setConstantInfoZero(int w = -1);
  bool isFakeArray() { return dimension.size() == 1 && dimension[0] == 1; }
  void display();
  size_t arrayEntryNum() { size_t num = 1; for (size_t idx : dimension) num *= idx; return num; }
  valInfo* computeConstant();
  void invalidArrayOptimize();
  void fillArrayInvalid(ExpTree* tree);
  uint64_t keyHash();
  NodeComponent* inferComponent();
  NodeComponent* reInferComponent();
  void updateTreeWithNewWIdth();
  int repOpCount();
  void updateIsRoot();
  void updateHeadTail();
  bool isLocal();
};

enum SuperType {
  SUPER_VALID,
  SUPER_EXTMOD,
  SUPER_ASYNC_RESET,
  SUPER_UINT_RESET,
  SUPER_UPDATE_REG,
};

enum SuperInfo {
  SUPER_INFO_IF,     // start indent
  SUPER_INFO_ELSE,   // dedent and then indent
  SUPER_INFO_DEDENT,
  SUPER_INFO_STR,
  SUPER_INFO_ASSIGN_BEG,
  SUPER_INFO_ASSIGN_END
};

class InstInfo{
public:
  SuperInfo infoType = SUPER_INFO_STR;
  std::string inst;
  Node* node;
  InstInfo(SuperInfo _type, Node* _node) {
    infoType = _type;
    node = _node;
  }
  InstInfo(std::string _inst, SuperInfo _type = SUPER_INFO_STR) {
    infoType = _type;
    inst = _inst;
  }
  InstInfo(SuperInfo _infoType, Node* _node, std::string _inst) {
    node = _node;
    inst = _inst;
    infoType = _infoType;
  }
};
class SuperNode {
private:
  static int counter;  // initialize to 1
public:
  std::string name;
  /* adjacent superNodes */
  std::set<SuperNode*> prev;
  std::set<SuperNode*> next;
  /* dependent but not adjacent */
  std::set<SuperNode*> depPrev;
  std::set<SuperNode*> depNext;
  std::vector<Node*> member; // The order of member is neccessary
  std::vector<InstInfo> insts;
  StmtTree* stmtTree = nullptr;
  int id;
  int order;
  int cppId = -1;
  SuperType superType = SUPER_VALID;
  Node* resetNode = nullptr;
  int localTmpNum = 0;
  SuperNode() {
    id = counter ++;
  }
  SuperNode(Node* _member) {
    member.push_back(_member);
    id = counter ++;
  }
  void add_member(Node* _member) {
    Assert(std::find(member.begin(), member.end(), _member) == member.end(), "member %s is already in superNode %d", _member->name.c_str(), id);
    member.push_back(_member);
    _member->set_super(this);
  }
  void connectPrev(SuperNode* _prev) {
    addPrev(_prev);
    _prev->addNext(this);
  }
  void connectNext(SuperNode* _next) {
    addNext(_next);
    _next->addPrev(this);
  }
  bool instsEmpty();
  void display();
  int findIndex(Node* node) {
    for (size_t ret = 0; ret < member.size(); ret ++) {
      if (member[ret] == node) return ret;
    }
    node->super->display();
    node->display();
    Panic();
  }
  void clear_relation();
  void addPrev(SuperNode* super);
  void addPrev(std::set<SuperNode*>& super);
  void erasePrev(SuperNode* super);
  void addNext(SuperNode* super);
  void addNext(std::set<SuperNode*>& super);
  void eraseNext(SuperNode* super);
  void reorderMember();
};

#endif
