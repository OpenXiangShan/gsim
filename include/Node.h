/*
  nodes in circuit graph
*/

#ifndef NODE_H
#define NODE_H

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
  NODE_MEM_MEMBER,
  NODE_OTHERS,
  NODE_ARRAY_MEMBER,
};

enum NodeStatus{ VALID_NODE, DEAD_NODE, CONSTANT_NODE, MERGED_NODE, DEAD_SRC };
enum IndexType{ INDEX_INT, INDEX_NODE };

enum ReaderMember { READER_ADDR = 0, READER_EN, READER_CLK, READER_DATA, READER_MEMBER_NUM};
enum WriterMember { WRITER_ADDR = 0, WRITER_EN, WRITER_CLK, WRITER_DATA, WRITER_MASK, WRITER_MEMBER_NUM};
enum ReadWriterMember { READWRITER_ADDR = 0, READWRITER_EN, READWRITER_CLK,
                      READWRITER_RDATA, READWRITER_WDATA, READWRITER_WMASK,
                      READWRITER_WMODE, READWRITER_MEMBER_NUM};


class TypeInfo {
public:
  int width;
  bool sign;
  std::vector<int> dimension;
  std::vector<std::pair<Node*, bool>> aggrMember; // all members, if not empty, all other infos above are invalid
  std::vector<AggrParentNode*> aggrParent; // the later the outer

  void add(Node* node, bool isFlip) { aggrMember.push_back(std::make_pair(node, isFlip)); }
  void set_width(int _width) { width = _width; }
  void set_sign(bool _sign) { sign = _sign; }
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
  void finialConnect(std::string lvalue, valInfo* info);
 public:

  Node(NodeType _type = NODE_OTHERS) {
    type    = _type;
  }

  std::string name;  // concat the module name in order (member in structure / temp variable)
  int id = -1;
  NodeType type;
  int width = -1;
  bool sign = false;
  int usedBit = -1;
  NodeStatus status = VALID_NODE;
  std::vector<int> dimension;

  std::set<Node*> next;
  std::set<Node*> prev;
  ExpTree* valTree = nullptr;
  ExpTree* resetCond = nullptr;  // valid in reg_src
  ExpTree* resetVal = nullptr;   // valid in reg_src
  std::vector<ExpTree*> arrayVal;

  SuperNode* super = nullptr;
/* used for memory */
  int rlatency;
  int wlatency;
  int depth;
  std::vector<Node*> member;
  Node* parent = nullptr;
/* used for registers */
  Node* regNext;
  bool regSplit = true;
/* used for instGerator */
  valInfo* computeInfo = nullptr;
/* used for splitted array */
  std::vector<Node*>arrayMember;
  Node* arrayParent = nullptr;
/* used for visitWhen in AST2Graph */

/* used in cppEmitter */
  std::set<int> nextActiveId;

  std::vector<std::string> insts;
  void set_id(int _id) {
    id     = _id;
  }
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
    Assert(type == NODE_REG_SRC || type == NODE_REG_DST, "The node is not register");
    if (type == NODE_REG_SRC) return this->regNext;
    return this;
  }
  Node* getSrc () {
    Assert(type == NODE_REG_SRC || type == NODE_REG_DST, "The node is not register");
    if (type == NODE_REG_DST) return this->regNext;
    return this;
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
  void set_member(int id, Node* node) {
    Assert(id < (int)member.size(), "id %d is out of bound [0, %ld)", id, member.size());
    member[id] = node;
    node->set_parent(this);
  }
  Node* get_member(int id) {
    Assert(id < (int)member.size(), "id %d is out of bound [0, %ld)", id, member.size());
    return member[id];
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
  void addArrayVal(ExpTree* val) {
    arrayVal.push_back(val);
  }
  Node* getArrayMember(int idx) {
    Assert(idx < (int)arrayMember.size(), "idx %d out of bound [0, %ld)", idx, arrayMember.size());
    return arrayMember[idx];
  }
  void set_super(SuperNode* _super) {
    super = _super;
  }
  void update_usedBit(int bits) {
    usedBit = MIN(width, MAX(bits, usedBit));
  }
  void updateConnect();
  void inferWidth();
  void addReset();
  void constructSuperNode(); // alloc superNode for every node
  void constructSuperConnect(); // connect superNode
  valInfo* compute(); // compute node
  valInfo* computeArray();
  void recompute();
  void passWidthToPrev();
  void splitArray();
  std::string arrayMemberName(int idx);
  ENode* isAlias();
  bool anyExtEdge();
  bool needActivate();
  void updateActivate();
  void removeConnection();
};

class SuperNode {
private:
  static int counter;  // initialize to 1
public:
  std::string name;
  std::set<SuperNode*> prev;
  std::set<SuperNode*> next;
  std::vector<Node*> member; // The order of member is neccessary
  int id;
  int order;
  int cppId = -1;
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
  void add_prev(SuperNode* _prev) {
    prev.insert(_prev);
    _prev->next.insert(this);
  }
  void add_next(SuperNode* _next) {
    next.insert(_next);
    _next->prev.insert(this);
  }
  bool instsEmpty();
  void display();
  int findIndex(Node* node) {
    for (size_t ret = 0; ret < member.size(); ret ++) {
      if (member[ret] == node) return ret;
    }
    Panic();
  }
};

#endif
