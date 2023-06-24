/**
 * @file PNode.h
 * @brief Declaration of PNode and PList classes.
 */

#ifndef PNODE_H
#define PNODE_H
#include "common.h"

/**
 * @class PList
 * @brief A class representing a list of PNodes.
 *
 */
class PList;

/**
 * @class Node
 * @brief A class representing a node in a parse tree.
 *
 */
class Node;

/**
 * @brief An enumeration of the types of PNodes.
 */
enum {
  P_INVALID, /**< An invalid node. */
  P_CIRCUIT, /**< A circuit node. */
  P_CIR_MODS, /**< A  circuit module node. */
  P_MOD, /**< A module node. */
  P_EXTMOD, /**< An external module node. */
  P_INTMOD, /**< An internal module node. */
  P_PORTS, /**< A ports node. */
  P_INPUT, /**< An input node. */
  P_OUTPUT, /**< An output node. */
  P_WIRE_DEF,
  P_REG_DEF,
  P_INST,
  P_NODE,
  P_CONNECT,
  P_PAR_CONNECT,
  P_WHEN,
  P_ELSE,
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
  P_REF_IDX,
  P_EXPR_INT,
  P_2EXPR,
  P_1EXPR,
  P_1EXPR1INT,
  P_1EXPR2INT,
  P_FIELD,
  P_FLIP_FIELD,
  P_AG_TYPE,
  P_AG_FIELDS,
  P_Clock,
  P_INT_TYPE,
  P_EXPR_INT_NOINIT,
  P_EXPR_INT_INIT,
  P_EXPR_MUX,
  P_STATEMENTS,
  P_PRINTF,
  P_EXPRS,
  P_ASSERT
};

enum { VALID_PNODE, CONSTANT_PNODE };

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
  PNode(int _type) { type = _type; }

  /**
   * @brief Constructor with a string parameter.
   *
   * @param str The string value of the node.
   */
  PNode(char* str) { name = std::string(str); }

  /**
   * @brief A vector of child nodes.
   */
  std::vector<PNode*> child;
  std::string info;
  std::string name;
  std::vector<std::string> extraInfo;
  int type;
  int width;
  bool sign;

  /**
   * @brief set valid_node to CONSTANT_PNODE
   */
  int status;
  /**
   * @brief CONSTANT_PNODE value
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

PNode* newNode(int type, char* info, char* name, int num, ...);
PNode* newNode(int type, char* name, int num, ...);
PNode* newNode(int type, char* info, char* name, PList* plist);

#endif
