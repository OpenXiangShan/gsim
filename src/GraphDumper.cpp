#include "common.h"
#include "ExecutableGrhEffects.h"
#include <set>
#include <map>
#include <stack>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <unordered_set>
#include <vector>
#include <unordered_map>

#ifndef GSIM_VERSION
#define GSIM_VERSION "UNKNOWN"
#endif

#ifndef GSIM_BUILD_DATE
#define GSIM_BUILD_DATE "UNKNOWN"
#endif

static std::vector<const Node*> collectGraphNodes(const graph* g) {
  std::vector<const Node*> nodes;
  if (!g->sortedSuper.empty()) {
    for (const SuperNode* super : g->sortedSuper) {
      for (const Node* node : super->member) nodes.push_back(node);
    }
    return nodes;
  }
  if (!g->allNodes.empty()) {
    nodes.reserve(g->allNodes.size());
    for (const Node* node : g->allNodes) nodes.push_back(node);
    return nodes;
  }
  std::unordered_set<const Node*> visited;
  for (const SuperNode* super : g->supersrc) {
    for (const Node* node : super->member) {
      if (visited.insert(node).second) nodes.push_back(node);
    }
  }
  return nodes;
}

class GraphDumper {
 public:
  GraphDumper(std::ostream& os) : os(os) {}

  void dump(const graph* g);

 private:
  template <typename T>
  void dumpSuperNode(const T& Super);

  void dump(const SuperNode* N);

  std::string& fixName(std::string& Name);

private:
  const graph* Root;

  std::ostream& os;
};

std::string& GraphDumper::fixName(std::string& Name) {
  std::replace(Name.begin(), Name.end(), '$', '_');
  return Name;
}

void GraphDumper::dump(const graph* g) {
  Root = g;

  os << "digraph GSimGraph {\n";

  if (!g->sortedSuper.empty())
    dumpSuperNode(g->sortedSuper);  // Used after toposort
  else
    dumpSuperNode(g->supersrc);  // Used before toposort

  os << "}\n";
}

template <typename T>
void GraphDumper::dumpSuperNode(const T& Super) {
  for (auto* N : Super) dump(N);

  for (auto* Super : Super)
    for (auto* Member : Super->member)
      for (auto* Next : Member->next) os << "\t\t\"" << Member->name << "\" -> \"" << Next->name << "\";\n";
}

void GraphDumper::dump(const SuperNode* N) {
  os << "\tsubgraph cluster_" << N->id << "{\n";

  for (auto* node : N->member) {
    auto& Name = node->name;

    os << "\t\t\"" << Name << "\"\n";
  }

  os << "\t}\n";
}

static std::string jsonEscape(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 8);
  for (unsigned char c : in) {
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}

static std::string nodeTypeToStr(NodeType t) {
  switch (t) {
    case NODE_INVALID: return "NODE_INVALID";
    case NODE_REG_SRC: return "NODE_REG_SRC";
    case NODE_REG_DST: return "NODE_REG_DST";
    case NODE_SPECIAL: return "NODE_SPECIAL";
    case NODE_INP: return "NODE_INP";
    case NODE_OUT: return "NODE_OUT";
    case NODE_MEMORY: return "NODE_MEMORY";
    case NODE_READER: return "NODE_READER";
    case NODE_WRITER: return "NODE_WRITER";
    case NODE_READWRITER: return "NODE_READWRITER";
    case NODE_INFER: return "NODE_INFER";
    case NODE_OTHERS: return "NODE_OTHERS";
    case NODE_REG_RESET: return "NODE_REG_RESET";
    case NODE_EXT_IN: return "NODE_EXT_IN";
    case NODE_EXT_OUT: return "NODE_EXT_OUT";
    case NODE_EXT: return "NODE_EXT";
    default: return "NODE_UNKNOWN";
  }
}

static const char* opTypeToStr(OPType t) {
  switch (t) {
    case OP_EMPTY: return "OP_EMPTY";
    case OP_MUX: return "OP_MUX";
    case OP_ADD: return "OP_ADD";
    case OP_SUB: return "OP_SUB";
    case OP_MUL: return "OP_MUL";
    case OP_DIV: return "OP_DIV";
    case OP_REM: return "OP_REM";
    case OP_LT: return "OP_LT";
    case OP_LEQ: return "OP_LEQ";
    case OP_GT: return "OP_GT";
    case OP_GEQ: return "OP_GEQ";
    case OP_EQ: return "OP_EQ";
    case OP_NEQ: return "OP_NEQ";
    case OP_DSHL: return "OP_DSHL";
    case OP_DSHR: return "OP_DSHR";
    case OP_AND: return "OP_AND";
    case OP_OR: return "OP_OR";
    case OP_XOR: return "OP_XOR";
    case OP_CAT: return "OP_CAT";
    case OP_ASUINT: return "OP_ASUINT";
    case OP_ASSINT: return "OP_ASSINT";
    case OP_ASCLOCK: return "OP_ASCLOCK";
    case OP_ASASYNCRESET: return "OP_ASASYNCRESET";
    case OP_CVT: return "OP_CVT";
    case OP_NEG: return "OP_NEG";
    case OP_NOT: return "OP_NOT";
    case OP_ANDR: return "OP_ANDR";
    case OP_ORR: return "OP_ORR";
    case OP_XORR: return "OP_XORR";
    case OP_PAD: return "OP_PAD";
    case OP_SHL: return "OP_SHL";
    case OP_SHR: return "OP_SHR";
    case OP_HEAD: return "OP_HEAD";
    case OP_TAIL: return "OP_TAIL";
    case OP_BITS: return "OP_BITS";
    case OP_BITS_NOSHIFT: return "OP_BITS_NOSHIFT";
    case OP_INDEX_INT: return "OP_INDEX_INT";
    case OP_INDEX: return "OP_INDEX";
    case OP_WHEN: return "OP_WHEN";
    case OP_PRINTF: return "OP_PRINTF";
    case OP_ASSERT: return "OP_ASSERT";
    case OP_EXIT: return "OP_EXIT";
    case OP_INT: return "OP_INT";
    case OP_GROUP: return "OP_GROUP";
    case OP_READ_MEM: return "OP_READ_MEM";
    case OP_WRITE_MEM: return "OP_WRITE_MEM";
    case OP_INFER_MEM: return "OP_INFER_MEM";
    case OP_INVALID: return "OP_INVALID";
    case OP_RESET: return "OP_RESET";
    case OP_SEXT: return "OP_SEXT";
    case OP_EXT_FUNC: return "OP_EXT_FUNC";
    case OP_STMT_SEQ: return "OP_STMT_SEQ";
    case OP_STMT_WHEN: return "OP_STMT_WHEN";
    case OP_STMT_NODE: return "OP_STMT_NODE";
    default: return "OP_UNKNOWN";
  }
}

static std::string nodeStatusToStr(NodeStatus status) {
  switch (status) {
    case VALID_NODE: return "VALID_NODE";
    case DEAD_NODE: return "DEAD_NODE";
    case CONSTANT_NODE: return "CONSTANT_NODE";
    case REPLICATION_NODE: return "REPLICATION_NODE";
    case SPLITTED_NODE: return "SPLITTED_NODE";
    case EMPTY_REG: return "EMPTY_REG";
    default: return "NODE_STATUS_UNKNOWN";
  }
}

struct StructuralActivationStats {
  size_t activationEdges = 0;
  size_t boundaryActivationEdges = 0;
  size_t selfActivationEdges = 0;
  size_t activeSourceNodes = 0;
  size_t boundaryActiveSourceNodes = 0;
  size_t alwaysActiveSupernodes = 0;
  std::set<std::pair<const SuperNode*, const SuperNode*>> uniqueActivationPairs;
  std::set<std::pair<const SuperNode*, const SuperNode*>> uniqueBoundaryActivationPairs;
  std::map<std::string, size_t> activationEdgesByNodeType;
  std::map<std::string, size_t> boundaryActivationEdgesByNodeType;
  std::map<std::string, size_t> activationSourceNodesByNodeType;
  std::map<std::string, size_t> boundaryActivationSourceNodesByNodeType;
  std::vector<size_t> activationTargetsPerSourceNode;
  std::vector<size_t> boundaryActivationTargetsPerSourceNode;
};

static bool isStructuralAlwaysActiveSupernode(const SuperNode* super) {
  return super && super->superType == SUPER_EXTMOD;
}

static StructuralActivationStats collectStructuralActivationStats(const graph* g) {
  StructuralActivationStats stats;
  std::unordered_set<const SuperNode*> validSupers;
  for (const SuperNode* super : g->sortedSuper) {
    if (!super) continue;
    validSupers.insert(super);
    if (isStructuralAlwaysActiveSupernode(super)) stats.alwaysActiveSupernodes ++;
  }

  auto addTarget = [&](const SuperNode* target, std::set<const SuperNode*>& targets) {
    if (!target) return;
    if (validSupers.find(target) == validSupers.end()) return;
    if (isStructuralAlwaysActiveSupernode(target)) return;
    targets.insert(target);
  };

  for (const SuperNode* super : g->sortedSuper) {
    if (!super) continue;
    for (const Node* node : super->member) {
      if (!node || node->status != VALID_NODE) continue;
      std::set<const SuperNode*> targets;
      for (const Node* nextNode : node->next) {
        if (!nextNode || !nextNode->super) continue;
        if (nextNode->super != super) {
          addTarget(nextNode->super, targets);
        } else if (node->orderInSuper >= nextNode->orderInSuper) {
          addTarget(super, targets);
        }
      }
      if (node->type == NODE_REG_DST && node->regNext) {
        addTarget(node->regNext->super, targets);
      }
      if (node->type == NODE_WRITER && node->parent) {
        for (const Node* port : node->parent->member) {
          if (port && port->type == NODE_READER && port->status == VALID_NODE) {
            addTarget(port->super, targets);
          }
        }
      }
      if (node->type == NODE_READWRITER && node->parent) {
        for (const Node* port : node->parent->member) {
          if (!port) continue;
          if (port == node) {
            if (port->parent && port->parent->extraInfo != "new") addTarget(super, targets);
          } else if ((port->type == NODE_READER || port->type == NODE_READWRITER) &&
                     port->status == VALID_NODE) {
            addTarget(port->super, targets);
          }
        }
      }

      if (targets.empty()) continue;
      const std::string nodeTypeName = nodeTypeToStr(node->type);
      size_t boundaryTargets = 0;
      stats.activeSourceNodes ++;
      stats.activationEdges += targets.size();
      stats.activationTargetsPerSourceNode.push_back(targets.size());
      stats.activationEdgesByNodeType[nodeTypeName] += targets.size();
      stats.activationSourceNodesByNodeType[nodeTypeName] ++;
      for (const SuperNode* target : targets) {
        stats.uniqueActivationPairs.insert({super, target});
        if (target == super) {
          stats.selfActivationEdges ++;
          continue;
        }
        boundaryTargets ++;
        stats.boundaryActivationEdges ++;
        stats.uniqueBoundaryActivationPairs.insert({super, target});
        stats.boundaryActivationEdgesByNodeType[nodeTypeName] ++;
      }
      if (boundaryTargets != 0) {
        stats.boundaryActiveSourceNodes ++;
        stats.boundaryActivationTargetsPerSourceNode.push_back(boundaryTargets);
        stats.boundaryActivationSourceNodesByNodeType[nodeTypeName] ++;
      }
    }
  }
  return stats;
}

class GraphJsonDumper {
 public:
  GraphJsonDumper(std::ostream& os) : os(os) {}
  void dump(const graph* g) {
    std::vector<const Node*> nodes = collectGraphNodes(g);
    std::unordered_set<const Node*> included(nodes.begin(), nodes.end());
    for (const Node* memory : g->memory) {
      if (memory && included.insert(memory).second) nodes.push_back(memory);
    }
    std::set<std::pair<std::string, std::string>> edges;
    os << "{\n  \"nodes\": [\n";
    bool firstNode = true;
    for (const Node* node : nodes) {
      if (!firstNode) os << ",\n";
      firstNode = false;
      os << "    {\"id\": " << node->id
         << ", \"name\": \"" << jsonEscape(node->name) << "\", "
         << "\"type\": \"" << nodeTypeToStr(node->type) << "\", "
         << "\"status\": \"" << nodeStatusToStr(node->status) << "\", "
         << "\"width\": " << node->width << ", "
         << "\"sign\": " << (node->sign ? "true" : "false") << ", "
         << "\"super\": " << (node->super ? node->super->id : -1)
         << ", \"parent\": " << (node->parent ? node->parent->id : -1)
         << ", \"clock\": " << (node->clock ? node->clock->id : -1)
         << ", \"depth\": " << node->depth
         << ", \"rlatency\": " << node->rlatency
         << ", \"wlatency\": " << node->wlatency
         << ", \"extraInfo\": \"" << jsonEscape(node->extraInfo) << "\""
         << ", \"dimension\": [";
      for (size_t i = 0; i < node->dimension.size(); i ++) {
        if (i) os << ", ";
        os << node->dimension[i];
      }
      os << "], \"members\": [";
      for (size_t i = 0; i < node->member.size(); i ++) {
        if (i) os << ", ";
        os << (node->member[i] ? node->member[i]->id : -1);
      }
      os << "]";
      if (globalConfig.DumpAssignTree) {
        os << ", \"assignTrees\": ";
        dumpAssignTrees(node);
        os << ", \"memTree\": ";
        if (node->memTree && node->memTree->getRoot()) dumpSingleAssignTree(node->memTree);
        else os << "null";
        os << ", \"effectClock\": ";
        if (node->effectClock) {
          ExpTree effectClockTree(node->effectClock);
          dumpSingleAssignTree(&effectClockTree);
        } else {
          os << "null";
        }
      }
      os << "}";
      for (const Node* next : node->next) edges.insert({node->name, next->name});
    }
    os << "\n  ],\n  \"edges\": [\n";
    bool firstEdge = true;
    for (const auto& e : edges) {
      if (!firstEdge) os << ",\n";
      firstEdge = false;
      os << "    [\"" << e.first << "\", \"" << e.second << "\"]";
    }
    os << "\n  ]\n}\n";
  }

 private:
  void dumpAssignTrees(const Node* node) {
    os << "[";
    bool firstTree = true;
    for (const ExpTree* tree : node->assignTree) {
      if (!tree || !tree->getRoot()) continue;
      if (!firstTree) os << ", ";
      firstTree = false;
      dumpSingleAssignTree(tree);
    }
    os << "]";
  }

  void dumpSingleAssignTree(const ExpTree* tree) {
    std::unordered_map<const ENode*, int> idMap;
    std::vector<const ENode*> order;
    auto addNode = [&](const ENode* enode) {
      if (!enode) return;
      if (idMap.find(enode) != idMap.end()) return;
      int id = static_cast<int>(order.size());
      idMap[enode] = id;
      order.push_back(enode);
    };

    std::vector<const ENode*> stack;
    addNode(tree->getRoot());
    addNode(tree->getlval());
    if (tree->getRoot()) stack.push_back(tree->getRoot());
    if (tree->getlval()) stack.push_back(tree->getlval());
    while (!stack.empty()) {
      const ENode* cur = stack.back();
      stack.pop_back();
      for (const ENode* child : cur->child) {
        if (!child) continue;
        if (idMap.find(child) == idMap.end()) {
          addNode(child);
          stack.push_back(child);
        }
      }
    }

    os << "{\"root\": " << (tree->getRoot() ? idMap[tree->getRoot()] : -1)
       << ", \"lvalue\": " << (tree->getlval() ? idMap[tree->getlval()] : -1)
       << ", \"nodes\": [\n";
    bool firstNode = true;
    for (const ENode* enode : order) {
      if (!firstNode) os << ",\n";
      firstNode = false;
      os << "      {\"id\": " << idMap[enode]
         << ", \"op\": \"" << opTypeToStr(enode->opType) << "\""
         << ", \"width\": " << enode->width
         << ", \"sign\": " << (enode->sign ? 1 : 0)
         << ", \"isClock\": " << (enode->isClock ? 1 : 0)
         << ", \"reset\": " << static_cast<int>(enode->reset);
      if (enode->nodePtr) {
        os << ", \"node\": \"" << jsonEscape(enode->nodePtr->name) << "\"";
      }
      if (enode->memoryNode) {
        os << ", \"memory\": \"" << jsonEscape(enode->memoryNode->name) << "\"";
      }
      if (!enode->values.empty()) {
        os << ", \"values\": [";
        for (size_t i = 0; i < enode->values.size(); i ++) {
          if (i) os << ", ";
          os << enode->values[i];
        }
        os << "]";
      }
      if (!enode->strVal.empty()) {
        os << ", \"strVal\": \"" << jsonEscape(enode->strVal) << "\"";
      }
      os << ", \"children\": [";
      bool firstChild = true;
      for (const ENode* child : enode->child) {
        if (!child) continue;
        if (!firstChild) os << ", ";
        firstChild = false;
        os << idMap[child];
      }
      os << "]}";
    }
    os << "\n    ]}";
  }

  std::ostream& os;
};

constexpr const char* kPreCoarsenProjectionFormat = "gsim.precoarsen-graph.v1";

static void writeJsonString(std::ostream& os, const std::string& value) {
  os << '"' << jsonEscape(value) << '"';
}

static void writeJsonString(std::ostream& os, const char* value) {
  writeJsonString(os, std::string(value));
}

static void writeAttrPrefix(std::ostream& os, bool& first, const char* key) {
  if (!first) os << ", ";
  first = false;
  writeJsonString(os, key);
  os << ": ";
}

static void writeBoolAttr(std::ostream& os, bool& first, const char* key, bool value) {
  writeAttrPrefix(os, first, key);
  os << "{\"t\": \"bool\", \"v\": " << (value ? "true" : "false") << "}";
}

static void writeIntAttr(std::ostream& os, bool& first, const char* key, int64_t value) {
  writeAttrPrefix(os, first, key);
  os << "{\"t\": \"int\", \"v\": " << value << "}";
}

static void writeStringAttr(std::ostream& os, bool& first, const char* key, const std::string& value) {
  writeAttrPrefix(os, first, key);
  os << "{\"t\": \"string\", \"v\": ";
  writeJsonString(os, value);
  os << "}";
}

static void writeIntVectorAttr(std::ostream& os, bool& first, const char* key,
                               const std::vector<const Node*>& nodes) {
  writeAttrPrefix(os, first, key);
  os << "{\"t\": \"int[]\", \"vs\": [";
  for (size_t i = 0; i < nodes.size(); i ++) {
    if (i) os << ", ";
    os << nodes[i]->id;
  }
  os << "]}";
}

static std::string preCoarsenValueSymbol(const Node* node) {
  return "gsim.v." + std::to_string(node->id);
}

static std::string preCoarsenOpSymbol(const Node* node) {
  return "gsim.assign." + std::to_string(node->id);
}

static std::string preCoarsenTerminalValueSymbol(const Node* node) {
  return "gsim.terminal." + std::to_string(node->id);
}

static std::string preCoarsenInputPortName(const Node* node) {
  return "gsim.in." + std::to_string(node->id);
}

static std::string preCoarsenOutputPortName(const Node* node) {
  return "gsim.out." + std::to_string(node->id);
}

static bool preCoarsenNodeOrder(const Node* lhs, const Node* rhs) {
  if (lhs->order != rhs->order) return lhs->order < rhs->order;
  return lhs->id < rhs->id;
}

static void sortProjectionNodes(std::vector<const Node*>& nodes) {
  std::sort(nodes.begin(), nodes.end(), preCoarsenNodeOrder);
  nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
}

struct PreCoarsenProjectionNode {
  const Node* node = nullptr;
  std::vector<const Node*> dataInputs;
  std::vector<const Node*> dependencyInputs;
  bool isSource = false;
  bool isTerminal = false;
};

static bool isPreCoarsenForcedTerminal(const Node* node) {
  switch (node->type) {
    case NODE_OUT:
    case NODE_SPECIAL:
    case NODE_EXT:
    case NODE_EXT_OUT:
      return true;
    default:
      return false;
  }
}

static void populatePreCoarsenProjectionNode(
    PreCoarsenProjectionNode& entry, const Node* node,
    const std::unordered_set<const Node*>& validNodes) {
  entry.node = node;
  entry.dataInputs.clear();
  entry.dependencyInputs.clear();
  for (const Node* prev : node->prev) {
    if (prev && validNodes.find(prev) != validNodes.end()) entry.dataInputs.push_back(prev);
  }
  for (const Node* prev : node->depPrev) {
    if (prev && validNodes.find(prev) != validNodes.end()) entry.dependencyInputs.push_back(prev);
  }
  sortProjectionNodes(entry.dataInputs);
  sortProjectionNodes(entry.dependencyInputs);
  entry.isSource = entry.dependencyInputs.empty();

  bool hasValidSuccessor = false;
  for (const Node* next : node->depNext) {
    if (next && validNodes.find(next) != validNodes.end()) {
      hasValidSuccessor = true;
      break;
    }
  }
  entry.isTerminal = !hasValidSuccessor || isPreCoarsenForcedTerminal(node);
}

static void writePreCoarsenNodeAttrs(std::ostream& os,
                                     const PreCoarsenProjectionNode& entry,
                                     const char* role) {
  const Node* node = entry.node;
  bool first = true;
  os << "{";
  writeBoolAttr(os, first, "analysis_only", true);
  writeStringAttr(os, first, "projection.format", kPreCoarsenProjectionFormat);
  writeStringAttr(os, first, "projection.role", role);
  writeIntAttr(os, first, "gsim.node_id", node->id);
  writeStringAttr(os, first, "gsim.node_name", node->name);
  writeStringAttr(os, first, "gsim.node_type", nodeTypeToStr(node->type));
  writeStringAttr(os, first, "gsim.node_status", nodeStatusToStr(node->status));
  writeIntAttr(os, first, "gsim.node_width", node->width);
  writeBoolAttr(os, first, "gsim.node_signed", node->sign);
  writeIntAttr(os, first, "gsim.node_used_bit", node->usedBit);
  writeIntAttr(os, first, "gsim.topo_order", node->order);
  writeIntAttr(os, first, "gsim.topo_order_in_super", node->orderInSuper);
  writeIntAttr(os, first, "gsim.supernode_id", node->super ? node->super->id : -1);
  writeIntAttr(os, first, "gsim.supernode_order", node->super ? node->super->order : -1);
  writeIntAttr(os, first, "gsim.source_line", node->lineno);
  writeBoolAttr(os, first, "gsim.is_source", entry.isSource);
  writeBoolAttr(os, first, "gsim.is_terminal", entry.isTerminal);
  writeIntVectorAttr(os, first, "gsim.data_input_ids", entry.dataInputs);
  writeIntVectorAttr(os, first, "gsim.dependency_input_ids", entry.dependencyInputs);
  os << "}";
}

static void writePreCoarsenValue(std::ostream& os,
                                 const PreCoarsenProjectionNode& entry) {
  const Node* node = entry.node;
  const int width = node->width > 0 ? node->width : 1;
  os << "{\"sym\": ";
  writeJsonString(os, preCoarsenValueSymbol(node));
  os << ", \"w\": " << width
     << ", \"sgn\": " << (node->sign ? "true" : "false")
     << ", \"type\": \"logic\""
     << ", \"in\": " << (entry.isSource ? "true" : "false")
     << ", \"out\": " << ((!entry.isSource && entry.isTerminal) ? "true" : "false")
     << ", \"inout\": false"
     << ", \"attrs\": ";
  writePreCoarsenNodeAttrs(os, entry, "value");
  os << "}";
}

static void writePreCoarsenTerminalValue(std::ostream& os,
                                         const PreCoarsenProjectionNode& entry) {
  const Node* node = entry.node;
  const int width = node->width > 0 ? node->width : 1;
  bool first = true;
  os << "{\"sym\": ";
  writeJsonString(os, preCoarsenTerminalValueSymbol(node));
  os << ", \"w\": " << width
     << ", \"sgn\": " << (node->sign ? "true" : "false")
     << ", \"type\": \"logic\""
     << ", \"in\": false, \"out\": true, \"inout\": false, \"attrs\": {";
  writeBoolAttr(os, first, "analysis_only", true);
  writeStringAttr(os, first, "projection.format", kPreCoarsenProjectionFormat);
  writeStringAttr(os, first, "projection.role", "terminal_proxy_value");
  writeIntAttr(os, first, "gsim.terminal_node_id", node->id);
  writeStringAttr(os, first, "gsim.terminal_node_name", node->name);
  os << "}}";
}

static void writePreCoarsenNodeOp(std::ostream& os,
                                  const PreCoarsenProjectionNode& entry) {
  const Node* node = entry.node;
  os << "{\"sym\": ";
  writeJsonString(os, preCoarsenOpSymbol(node));
  os << ", \"kind\": \"kAssign\", \"in\": [";
  for (size_t i = 0; i < entry.dependencyInputs.size(); i ++) {
    if (i) os << ", ";
    writeJsonString(os, preCoarsenValueSymbol(entry.dependencyInputs[i]));
  }
  os << "], \"out\": [";
  writeJsonString(os, preCoarsenValueSymbol(node));
  os << "], \"attrs\": ";
  writePreCoarsenNodeAttrs(os, entry, "node_assign");
  os << "}";
}

static void writePreCoarsenTerminalOp(std::ostream& os,
                                      const PreCoarsenProjectionNode& entry) {
  const Node* node = entry.node;
  bool first = true;
  os << "{\"sym\": ";
  writeJsonString(os, "gsim.terminal.assign." + std::to_string(node->id));
  os << ", \"kind\": \"kAssign\", \"in\": [";
  writeJsonString(os, preCoarsenValueSymbol(node));
  os << "], \"out\": [";
  writeJsonString(os, preCoarsenTerminalValueSymbol(node));
  os << "], \"attrs\": {";
  writeBoolAttr(os, first, "analysis_only", true);
  writeStringAttr(os, first, "projection.format", kPreCoarsenProjectionFormat);
  writeStringAttr(os, first, "projection.role", "terminal_proxy_assign");
  writeIntAttr(os, first, "gsim.terminal_node_id", node->id);
  writeStringAttr(os, first, "gsim.terminal_node_name", node->name);
  os << "}}";
}

void graph::exportPreCoarsenGrh(const std::string& path) {
  std::vector<const Node*> nodes;
  std::unordered_set<const Node*> seen;
  for (const SuperNode* super : sortedSuper) {
    if (!super) continue;
    for (const Node* node : super->member) {
      if (node && node->status == VALID_NODE && seen.insert(node).second) nodes.push_back(node);
    }
  }
  std::sort(nodes.begin(), nodes.end(), preCoarsenNodeOrder);

  std::unordered_set<const Node*> validNodes;
  validNodes.reserve(nodes.size());
  validNodes.insert(nodes.begin(), nodes.end());
  size_t dataEdgeCount = 0;
  size_t dependencyEdgeCount = 0;
  size_t terminalCount = 0;
  size_t sourceTerminalCount = 0;
  PreCoarsenProjectionNode scratch;

  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    dataEdgeCount += scratch.dataInputs.size();
    dependencyEdgeCount += scratch.dependencyInputs.size();
    if (scratch.isTerminal) {
      terminalCount ++;
      if (scratch.isSource) sourceTerminalCount ++;
    }
  }

  const std::string graphSymbol = name.empty() ? "gsim_precoarsen" : name;
  std::ofstream os(path);
  Assert(os.good(), "failed to open pre-coarsen GRH projection: %s", path.c_str());

  bool rootFirst = true;
  os << "{\n";
  os << "  \"format\": ";
  writeJsonString(os, kPreCoarsenProjectionFormat);
  os << ",\n  \"stage\": \"pre-coarsen\",\n  \"analysisOnly\": true,\n";
  os << "  \"attrs\": {";
  writeBoolAttr(os, rootFirst, "analysis_only", true);
  writeStringAttr(os, rootFirst, "projection.format", kPreCoarsenProjectionFormat);
  writeStringAttr(os, rootFirst, "gsim.stage", "pre-coarsen");
  writeStringAttr(os, rootFirst, "gsim.boundary", "PreCoarsen");
  writeStringAttr(os, rootFirst, "gsim.input_file", globalConfig.InputFile);
  writeIntAttr(os, rootFirst, "gsim.input_file_bytes", static_cast<int64_t>(globalConfig.InputFileBytes));
  writeStringAttr(os, rootFirst, "gsim.version", GSIM_VERSION);
  writeStringAttr(os, rootFirst, "gsim.build_date", GSIM_BUILD_DATE);
  writeIntAttr(os, rootFirst, "gsim.node_count", static_cast<int64_t>(nodes.size()));
  writeIntAttr(os, rootFirst, "gsim.data_edge_count", static_cast<int64_t>(dataEdgeCount));
  writeIntAttr(os, rootFirst, "gsim.dependency_edge_count", static_cast<int64_t>(dependencyEdgeCount));
  os << "},\n";
  os << "  \"gsim\": {\"format\": ";
  writeJsonString(os, kPreCoarsenProjectionFormat);
  os << ", \"stage\": \"pre-coarsen\", \"boundary\": \"PreCoarsen\", \"analysisOnly\": true, \"inputFile\": ";
  writeJsonString(os, globalConfig.InputFile);
  os << ", \"inputFileBytes\": " << globalConfig.InputFileBytes
     << ", \"version\": ";
  writeJsonString(os, GSIM_VERSION);
  os << ", \"buildDate\": ";
  writeJsonString(os, GSIM_BUILD_DATE);
  os << ", \"nodeCount\": " << nodes.size()
     << ", \"dataEdgeCount\": " << dataEdgeCount
     << ", \"dependencyEdgeCount\": " << dependencyEdgeCount
     << ", \"terminalCount\": " << terminalCount
     << "},\n";
  os << "  \"graphs\": [\n    {\n      \"symbol\": ";
  writeJsonString(os, graphSymbol);
  os << ",\n      \"attrs\": {";
  bool graphFirst = true;
  writeBoolAttr(os, graphFirst, "analysis_only", true);
  writeStringAttr(os, graphFirst, "projection.format", kPreCoarsenProjectionFormat);
  writeStringAttr(os, graphFirst, "gsim.stage", "pre-coarsen");
  writeStringAttr(os, graphFirst, "gsim.boundary", "PreCoarsen");
  writeStringAttr(os, graphFirst, "gsim.graph_name", name);
  os << "},\n      \"declaredSymbols\": [],\n      \"vals\": [";

  bool firstValue = true;
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (!firstValue) os << ",";
    os << "\n        ";
    writePreCoarsenValue(os, scratch);
    firstValue = false;
  }
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (!scratch.isSource || !scratch.isTerminal) continue;
    if (!firstValue) os << ",";
    os << "\n        ";
    writePreCoarsenTerminalValue(os, scratch);
    firstValue = false;
  }
  if (!firstValue) os << "\n      ";
  os << "],\n      \"ports\": {\n        \"in\": [";

  bool firstInputPort = true;
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (!scratch.isSource) continue;
    if (!firstInputPort) os << ", ";
    os << "{\"name\": ";
    writeJsonString(os, preCoarsenInputPortName(scratch.node));
    os << ", \"val\": ";
    writeJsonString(os, preCoarsenValueSymbol(scratch.node));
    os << "}";
    firstInputPort = false;
  }
  os << "],\n        \"out\": [";

  bool firstOutputPort = true;
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (!scratch.isTerminal) continue;
    if (!firstOutputPort) os << ", ";
    os << "{\"name\": ";
    writeJsonString(os, preCoarsenOutputPortName(scratch.node));
    os << ", \"val\": ";
    writeJsonString(os, scratch.isSource ? preCoarsenTerminalValueSymbol(scratch.node)
                                          : preCoarsenValueSymbol(scratch.node));
    os << "}";
    firstOutputPort = false;
  }
  os << "],\n        \"inout\": []\n      },\n      \"ops\": [";

  bool firstOp = true;
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (scratch.isSource) continue;
    if (!firstOp) os << ",";
    os << "\n        ";
    writePreCoarsenNodeOp(os, scratch);
    firstOp = false;
  }
  for (const Node* node : nodes) {
    populatePreCoarsenProjectionNode(scratch, node, validNodes);
    if (!scratch.isSource || !scratch.isTerminal) continue;
    if (!firstOp) os << ",";
    os << "\n        ";
    writePreCoarsenTerminalOp(os, scratch);
    firstOp = false;
  }
  if (!firstOp) os << "\n      ";
  os << "]\n    }\n  ],\n  \"declaredSymbols\": [],\n  \"tops\": [";
  writeJsonString(os, graphSymbol);
  os << "]\n}\n";
  Assert(os.good(), "failed to write pre-coarsen GRH projection: %s", path.c_str());
  printf("[PreCoarsenExport] path=%s nodes=%ld data_edges=%ld dependency_edges=%ld terminals=%ld source_terminal_proxies=%ld\n",
         path.c_str(), nodes.size(), dataEdgeCount, dependencyEdgeCount, terminalCount, sourceTerminalCount);
}

class GraphStatsJsonDumper {
 public:
  GraphStatsJsonDumper(std::ostream& os) : os(os) {}

  void dump(const graph* g) {
    const std::vector<const Node*> nodes = collectGraphNodes(g);
    const StructuralActivationStats activationStats = collectStructuralActivationStats(g);
    std::map<std::string, size_t> nodeTypes;
    std::map<std::string, size_t> nodeStatuses;
    std::map<std::string, size_t> treeSlots;
    std::map<std::string, size_t> enodeOps;
    std::map<std::string, size_t> nodeRefTargetTypes;
    std::unordered_set<const ENode*> visited;
    std::unordered_set<const SuperNode*> uniqueSupers;
    std::set<std::pair<const SuperNode*, const SuperNode*>> supernodeEdges;
    std::unordered_set<const SuperNode*> emittedSupers;
    std::set<std::pair<const SuperNode*, const SuperNode*>> emittedSupernodeEdges;
    size_t nodeCount = 0;
    size_t edgeCount = 0;
    size_t depEdgeCount = 0;
    size_t treeCount = 0;
    size_t treeRootCount = 0;
    size_t treeLvalueCount = 0;
    size_t enodeEdgeCount = 0;
    size_t nodeRefCount = 0;
    size_t intConstCount = 0;
    size_t maxChildren = 0;
    std::vector<size_t> enodesPerNode;
    std::vector<size_t> treeSlotsPerNode;
    std::vector<size_t> supernodeMemberSizes;
    std::vector<size_t> enodesPerSupernode;
    std::map<std::string, size_t> enodeDominantSlots;
    std::map<std::string, size_t> effectKinds;
    std::map<std::string, size_t> effectBaseClocks;
    std::map<std::string, size_t> effectRejections;
    size_t effectNodeCount = 0;
    size_t effectClockPresentCount = 0;
    size_t effectBaseClockResolvedCount = 0;
    size_t effectPlanSupportedCount = 0;
    size_t effectPlanOptimizerElidedCount = 0;

    auto collectENode = [&](const ENode* root) {
      if (!root) return;
      std::stack<const ENode*> stack;
      stack.push(root);
      while (!stack.empty()) {
        const ENode* cur = stack.top();
        stack.pop();
        if (!cur) continue;
        if (!visited.insert(cur).second) continue;
        maxChildren = std::max(maxChildren, cur->child.size());
        enodeEdgeCount += cur->child.size();
        if (cur->nodePtr) {
          nodeRefCount ++;
          nodeRefTargetTypes[nodeTypeToStr(cur->nodePtr->type)] ++;
        } else {
          enodeOps[opTypeToStr(cur->opType)] ++;
          if (cur->opType == OP_INT) intConstCount ++;
        }
        for (const ENode* child : cur->child) stack.push(child);
      }
    };

    auto collectTree = [&](const ExpTree* tree, const char* slot) {
      if (!tree) return;
      treeSlots[slot] ++;
      treeCount ++;
      if (tree->getRoot()) {
        treeRootCount ++;
        collectENode(tree->getRoot());
      }
      if (tree->getlval()) {
        treeLvalueCount ++;
        collectENode(tree->getlval());
      }
    };

    auto collectNodeExpression = [&](const ENode* root,
                                     std::unordered_set<const ENode*>& nodeVisited) {
      std::stack<const ENode*> stack;
      if (root) stack.push(root);
      while (!stack.empty()) {
        const ENode* cur = stack.top();
        stack.pop();
        if (!cur) continue;
        if (!nodeVisited.insert(cur).second) continue;
        for (const ENode* child : cur->child) stack.push(child);
      }
    };

    auto collectNodeTree = [&](const ExpTree* tree,
                               std::unordered_set<const ENode*>& nodeVisited) {
      if (!tree) return;
      collectNodeExpression(tree->getRoot(), nodeVisited);
      collectNodeExpression(tree->getlval(), nodeVisited);
    };

    auto collectNodeTreeSlot = [&](const ExpTree* tree,
                                   const char* slot,
                                   std::unordered_set<const ENode*>& nodeVisited) {
      if (!tree) return static_cast<size_t>(0);
      const size_t before = nodeVisited.size();
      collectNodeTree(tree, nodeVisited);
      return nodeVisited.size() - before;
    };

    auto collectNodeAllTrees = [&](const Node* node,
                                   std::unordered_set<const ENode*>& nodeVisited) {
      if (!node) return;
      for (const ExpTree* tree : node->assignTree) collectNodeTree(tree, nodeVisited);
      collectNodeTree(node->valTree, nodeVisited);
      collectNodeTree(node->resetTree, nodeVisited);
      collectNodeTree(node->resetCond, nodeVisited);
      collectNodeTree(node->resetVal, nodeVisited);
      collectNodeTree(node->memTree, nodeVisited);
      collectNodeExpression(node->effectClock, nodeVisited);
    };

    for (const SuperNode* super : g->sortedSuper) {
      if (!super) continue;
      supernodeMemberSizes.push_back(super->member.size());
      if (!super->insts.empty() || super->superType == SUPER_EXTMOD || super->superType == SUPER_ASYNC_RESET) {
        emittedSupers.insert(super);
      }
      for (const SuperNode* next : super->next) {
        if (next && next != super) supernodeEdges.insert({super, next});
      }
      std::unordered_set<const ENode*> superVisited;
      for (const Node* member : super->member) collectNodeAllTrees(member, superVisited);
      enodesPerSupernode.push_back(superVisited.size());
    }

    for (const Node* node : nodes) {
      nodeCount ++;
      if (node->super) uniqueSupers.insert(node->super);
      nodeTypes[nodeTypeToStr(node->type)] ++;
      nodeStatuses[nodeStatusToStr(node->status)] ++;
      edgeCount += node->next.size();
      depEdgeCount += node->depNext.size();
      for (const ExpTree* tree : node->assignTree) collectTree(tree, "assignTree");
      collectTree(node->valTree, "valTree");
      collectTree(node->resetTree, "resetTree");
      collectTree(node->resetCond, "resetCond");
      collectTree(node->resetVal, "resetVal");
      collectTree(node->memTree, "memTree");
      if (node->effectClock) {
        treeSlots["effectClock"] ++;
        treeCount ++;
        treeRootCount ++;
        collectENode(node->effectClock);
      }

      if (node->type == NODE_SPECIAL) {
        effectNodeCount ++;
        if (node->effectClock) effectClockPresentCount ++;
        const executable_grh::EffectPlan plan =
            executable_grh::resolveExecutableGrhEffect(*node);
        if (plan.status == executable_grh::EffectPlanStatus::Supported) {
          effectPlanSupportedCount ++;
          effectKinds[executable_grh::effectKindName(plan.kind)] ++;
          if (plan.baseClock) {
            effectBaseClockResolvedCount ++;
            effectBaseClocks[plan.baseClock->name] ++;
          }
        } else if (plan.status ==
                   executable_grh::EffectPlanStatus::OptimizerElided) {
          effectPlanOptimizerElidedCount ++;
        } else {
          effectRejections[plan.reason.empty() ? "unspecified" : plan.reason] ++;
        }
      }

      std::unordered_set<const ENode*> nodeVisited;
      std::map<std::string, size_t> slotCounts;
      size_t nodeTreeSlots = 0;
      auto accountSlot = [&](const ExpTree* tree, const char* slot) {
        if (!tree) return;
        nodeTreeSlots ++;
        slotCounts[slot] += collectNodeTreeSlot(tree, slot, nodeVisited);
      };
      for (const ExpTree* tree : node->assignTree) accountSlot(tree, "assignTree");
      accountSlot(node->valTree, "valTree");
      accountSlot(node->resetTree, "resetTree");
      accountSlot(node->resetCond, "resetCond");
      accountSlot(node->resetVal, "resetVal");
      accountSlot(node->memTree, "memTree");
      if (node->effectClock) {
        nodeTreeSlots ++;
        const size_t before = nodeVisited.size();
        collectNodeExpression(node->effectClock, nodeVisited);
        slotCounts["effectClock"] += nodeVisited.size() - before;
      }
      enodesPerNode.push_back(nodeVisited.size());
      treeSlotsPerNode.push_back(nodeTreeSlots);
      std::string dominantSlot = "none";
      size_t dominantCount = 0;
      for (const auto& [slot, count] : slotCounts) {
        if (count > dominantCount) {
          dominantSlot = slot;
          dominantCount = count;
        }
      }
      enodeDominantSlots[dominantSlot] ++;
    }
    for (const auto& edge : supernodeEdges) {
      if (emittedSupers.find(edge.first) != emittedSupers.end() &&
          emittedSupers.find(edge.second) != emittedSupers.end()) {
        emittedSupernodeEdges.insert(edge);
      }
    }

    os << "{\n"
       << "  \"graph\": \"" << jsonEscape(g->name) << "\",\n"
       << "  \"supernode_count\": " << uniqueSupers.size() << ",\n"
       << "  \"supernode_edge_count\": " << supernodeEdges.size() << ",\n"
       << "  \"activation_edges\": " << activationStats.activationEdges << ",\n"
       << "  \"boundary_activation_edges\": " << activationStats.boundaryActivationEdges << ",\n"
       << "  \"self_activation_edges\": " << activationStats.selfActivationEdges << ",\n"
       << "  \"unique_activation_edges\": " << activationStats.uniqueActivationPairs.size() << ",\n"
       << "  \"unique_boundary_activation_edges\": "
       << activationStats.uniqueBoundaryActivationPairs.size() << ",\n"
       << "  \"active_source_nodes\": " << activationStats.activeSourceNodes << ",\n"
       << "  \"boundary_active_source_nodes\": " << activationStats.boundaryActiveSourceNodes << ",\n"
       << "  \"always_active_supernodes\": " << activationStats.alwaysActiveSupernodes << ",\n"
       << "  \"emitted_supernode_count\": " << emittedSupers.size() << ",\n"
       << "  \"emitted_supernode_edge_count\": " << emittedSupernodeEdges.size() << ",\n"
       << "  \"node_count\": " << nodeCount << ",\n"
       << "  \"edge_count\": " << edgeCount << ",\n"
       << "  \"dep_edge_count\": " << depEdgeCount << ",\n"
       << "  \"node_types\": ";
    dumpMap(nodeTypes);
    os << ",\n  \"node_status\": ";
    dumpMap(nodeStatuses);
    os << ",\n  \"activation_edges_by_node_type\": ";
    dumpMap(activationStats.activationEdgesByNodeType);
    os << ",\n  \"boundary_activation_edges_by_node_type\": ";
    dumpMap(activationStats.boundaryActivationEdgesByNodeType);
    os << ",\n  \"activation_source_nodes_by_node_type\": ";
    dumpMap(activationStats.activationSourceNodesByNodeType);
    os << ",\n  \"boundary_activation_source_nodes_by_node_type\": ";
    dumpMap(activationStats.boundaryActivationSourceNodesByNodeType);
    os << ",\n  \"tree_slots\": ";
    dumpMap(treeSlots);
    os << ",\n  \"effects\": {\n"
       << "    \"node_count\": " << effectNodeCount << ",\n"
       << "    \"clock_present_count\": " << effectClockPresentCount << ",\n"
       << "    \"base_clock_resolved_count\": " << effectBaseClockResolvedCount << ",\n"
       << "    \"plan_supported_count\": " << effectPlanSupportedCount << ",\n"
       << "    \"plan_optimizer_elided_count\": "
       << effectPlanOptimizerElidedCount << ",\n"
       << "    \"kinds\": ";
    dumpMap(effectKinds);
    os << ",\n    \"base_clocks\": ";
    dumpMap(effectBaseClocks);
    os << ",\n    \"rejections\": ";
    dumpMap(effectRejections);
    os << "\n  }";
    os << ",\n  \"expnodes\": {\n"
       << "    \"tree_count\": " << treeCount << ",\n"
       << "    \"tree_root_count\": " << treeRootCount << ",\n"
       << "    \"tree_lvalue_count\": " << treeLvalueCount << ",\n"
       << "    \"unique_count\": " << visited.size() << ",\n"
       << "    \"edge_count\": " << enodeEdgeCount << ",\n"
       << "    \"max_children\": " << maxChildren << ",\n"
       << "    \"node_ref_count\": " << nodeRefCount << ",\n"
       << "    \"int_const_count\": " << intConstCount << ",\n"
       << "    \"op_types\": ";
    dumpMap(enodeOps);
    os << ",\n    \"node_ref_target_types\": ";
    dumpMap(nodeRefTargetTypes);
    os << "\n  },\n  \"nodes_enodes\": ";
    dumpVectorStats(enodesPerNode);
    os << ",\n  \"nodes_tree_slots\": ";
    dumpVectorStats(treeSlotsPerNode);
    os << ",\n  \"supernodes_members\": ";
    dumpVectorStats(supernodeMemberSizes);
    os << ",\n  \"supernodes_enodes\": ";
    dumpVectorStats(enodesPerSupernode);
    os << ",\n  \"node_enode_dominant_slots\": ";
    dumpMap(enodeDominantSlots);
    os << ",\n  \"activation_targets_per_source_node\": ";
    dumpVectorStats(activationStats.activationTargetsPerSourceNode);
    os << ",\n  \"boundary_activation_targets_per_source_node\": ";
    dumpVectorStats(activationStats.boundaryActivationTargetsPerSourceNode);
    os << "\n}\n";
  }

 private:
  void dumpVectorStats(std::vector<size_t> values) {
    std::sort(values.begin(), values.end());
    const size_t count = values.size();
    const size_t sum = std::accumulate(values.begin(), values.end(), static_cast<size_t>(0));
    auto percentile = [&](size_t num, size_t den) -> size_t {
      if (values.empty()) return 0;
      const size_t idx = (values.size() - 1) * num / den;
      return values[idx];
    };
    size_t zero = 0;
    for (size_t value : values) {
      if (value == 0) zero ++;
    }
    os << "{"
       << "\"count\": " << count
       << ", \"sum\": " << sum
       << ", \"zero\": " << zero
       << ", \"min\": " << (values.empty() ? 0 : values.front())
       << ", \"mean\": " << (count == 0 ? 0.0 : static_cast<double>(sum) / static_cast<double>(count))
       << ", \"median\": " << percentile(50, 100)
       << ", \"p90\": " << percentile(90, 100)
       << ", \"p99\": " << percentile(99, 100)
       << ", \"max\": " << (values.empty() ? 0 : values.back())
       << "}";
  }

  void dumpMap(const std::map<std::string, size_t>& values) {
    os << "{";
    bool first = true;
    for (const auto& it : values) {
      if (!first) os << ", ";
      first = false;
      os << "\"" << jsonEscape(it.first) << "\": " << it.second;
    }
    os << "}";
  }

  std::ostream& os;
};

void graph::dump(std::string FileName) {
  std::string prefix = globalConfig.OutputDir + "/" + this->name + "_" + FileName;
  if (globalConfig.DumpGraphDot) {
    std::ofstream ofs(prefix + ".dot");
    GraphDumper(ofs).dump(this);
  }
  if (globalConfig.DumpGraphJson) {
    std::ofstream ofs(prefix + ".json");
    GraphJsonDumper(ofs).dump(this);
  }
  if (globalConfig.DumpGraphStats) {
    std::ofstream ofs(prefix + "_Stats.json");
    GraphStatsJsonDumper(ofs).dump(this);
  }
}

bool graph::dumpStage(std::string stageName) {
  if (globalConfig.EnableDumpGraph &&
      (globalConfig.DumpStages.empty() || globalConfig.DumpStages.count(stageName))) {
    dump(stageName);
  }
  return !globalConfig.StopAfterStage.empty() && globalConfig.StopAfterStage == stageName;
}
