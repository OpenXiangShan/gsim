#include "common.h"
#include <set>
#include <map>
#include <stack>
#include <unordered_set>
#include <vector>
#include <unordered_map>

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

class GraphJsonDumper {
 public:
  GraphJsonDumper(std::ostream& os) : os(os) {}
  void dump(const graph* g) {
    const std::vector<const Node*> nodes = collectGraphNodes(g);
    std::set<std::pair<std::string, std::string>> edges;
    os << "{\n  \"nodes\": [\n";
    bool firstNode = true;
    for (const Node* node : nodes) {
      if (!firstNode) os << ",\n";
      firstNode = false;
      os << "    {\"name\": \"" << jsonEscape(node->name) << "\", "
         << "\"type\": \"" << nodeTypeToStr(node->type) << "\", "
         << "\"super\": " << (node->super ? node->super->id : -1);
      if (globalConfig.DumpAssignTree) {
        os << ", \"assignTrees\": ";
        dumpAssignTrees(node);
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

class GraphStatsJsonDumper {
 public:
  GraphStatsJsonDumper(std::ostream& os) : os(os) {}

  void dump(const graph* g) {
    const std::vector<const Node*> nodes = collectGraphNodes(g);
    std::map<std::string, size_t> nodeTypes;
    std::map<std::string, size_t> nodeStatuses;
    std::map<std::string, size_t> treeSlots;
    std::map<std::string, size_t> enodeOps;
    std::map<std::string, size_t> nodeRefTargetTypes;
    std::unordered_set<const ENode*> visited;
    std::unordered_set<const SuperNode*> uniqueSupers;
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
    }

    os << "{\n"
       << "  \"graph\": \"" << jsonEscape(g->name) << "\",\n"
       << "  \"supernode_count\": " << uniqueSupers.size() << ",\n"
       << "  \"node_count\": " << nodeCount << ",\n"
       << "  \"edge_count\": " << edgeCount << ",\n"
       << "  \"dep_edge_count\": " << depEdgeCount << ",\n"
       << "  \"node_types\": ";
    dumpMap(nodeTypes);
    os << ",\n  \"node_status\": ";
    dumpMap(nodeStatuses);
    os << ",\n  \"tree_slots\": ";
    dumpMap(treeSlots);
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
    os << "\n  }\n}\n";
  }

 private:
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
