/**
 * @file PNode.h
 * @brief Declaration of PNode and PList classes.
 */

#ifndef PNODE_H
#define PNODE_H

/**
 * @class PList
 * @brief A class representing a list of PNodes.
 *
 */
class PList;
class Node;

/* AST Node */
enum PNodeType{
  P_EMPTY,
  P_CIRCUIT,
  P_MOD,
  P_EXTMOD,
  P_INTMOD,
  P_PORTS,
  P_INPUT,
  P_OUTPUT,
  P_WIRE_DEF,
  P_REG_DEF,
  P_REG_RESET_DEF,
  P_INST,
  P_NODE,
  P_CONNECT,
  P_PAR_CONNECT,
  P_WHEN,
  P_MEMORY,
  P_READER,
  P_WRITER,
  P_READWRITER,
  P_RUW,
  P_RLATENCT,
  P_WLATENCT,
  P_DATATYPE,
  P_DEPTH,
  P_REF,
  P_REF_DOT,
  P_REF_IDX_INT,
  P_REF_IDX_EXPR,
  P_2EXPR,
  P_1EXPR,
  P_1EXPR1INT,
  P_1EXPR2INT,
  P_FIELD,
  P_FLIP_FIELD,
  P_AG_TYPE,
  P_AG_FIELDS,
  P_Clock,
  P_ASYRESET,
  P_INT_TYPE,
  P_EXPR_INT_NOINIT,
  P_EXPR_INT_INIT,
  P_EXPR_MUX,
  P_STATEMENTS,
  P_PRINTF,
  P_EXPRS,
  P_ASSERT,
  P_INDEX,
  P_CONS_INDEX,
  P_L_CONS_INDEX,
  P_L_INDEX,
  P_INVALID,
};

enum PNodeState { VALID_PNODE, CONSTANT_PNODE };

class AggrType;

/**
 * @class PNode
 * @brief A class representing a node in a parse tree.
 *
 */
class PNode {
 public:
  /**
   * @brief Default constructor.
   */
  PNode() {}

  /**
   * @brief Constructor with a type parameter.
   *
   * @param _type The type of the node.
   */
  PNode(PNodeType _type, int _lineno = -1) { type = _type; lineno = _lineno; }

  /**
   * @brief Constructor with a string parameter.
   *
   * @param str The string value of the node.
   */
  PNode(char* str, int _lineno = -1) { name = std::string(str); lineno = _lineno; }

  /**
   * @brief A vector of child nodes.
   */
  std::vector<PNode*> child;
  std::string info;
  std::string name;
  std::vector<std::string> extraInfo;
  PNodeType type;
  int width = 0;
  int lineno;
  bool sign = 0;
  // AggrType* aggrType = NULL;
  int id = -1;
  /**
   * @brief set valid_node to CONSTANT_PNODE
   */
  PNodeState status = VALID_PNODE;
  /**
   * @brief used for CONSTANT_PNODE
   */
  std::string consVal;

  void appendChild(PNode* p);
  void appendExtraInfo(char* info);
  void appendChildList(PList* plist);
  void setWidth(int _width);
  int getChildNum();
  PNode* getChild(int idx);
  int getExtraNum() { return extraInfo.size(); }
  std::string getExtra(int idx) { return extraInfo[idx]; }
  void setSign(bool s) { sign = s; }
};

/**
 * @class PList
 * @brief A class representing pnode list in a parse tree.
 *
 */
class PList {
 public:
  /**
   * @brief Constructor with a initial node parameter.
   *
   * @param pnode
   */
  PList(PNode* pnode) { siblings.push_back(pnode); }
  PList() {}

  std::vector<PNode*> siblings;

  void append(PNode* pnode);
  void append(int num, ...);
  void concat(PList* plist);
};

PNode* newNode(PNodeType type, int lineno, char* info, char* name, int num, ...);
PNode* newNode(PNodeType type, int lineno, char* name, int num, ...);
PNode* newNode(PNodeType type, int lineno, char* info, char* name, PList* plist);

#endif
