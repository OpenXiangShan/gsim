#include "common.h"
#include <set>
#include <map>
#include <stack>
#include <algorithm>
#include <numeric>
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

static void dumpSemantic(std::ostream& out, const Node* node) {
  const char* role = node->sourceKind == Node::SourceKind::WhenCondition ? "predicate" :
      node->sourceKind == Node::SourceKind::MemoryReadAddress ? "memory-read-address" :
      node->type == NODE_SPECIAL ? "effect" :
      node->type == NODE_MEMORY ? "memory-storage" :
      node->type == NODE_REG_SRC ? "register-current" :
      node->type == NODE_REG_DST ? "register-next" : "value";
  out << ",\"semantic\":{\"path\":[";
  for (size_t i = 0; i < node->semanticPath.size(); ++i) {
    if (i) out << ',';
    out << '\"' << jsonEscape(node->semanticPath[i]) << '\"';
  }
  out << "],\"scopeDepth\":" << node->semanticScopeDepth << ",\"role\":\"" << role << '\"';
  if (!node->sourceEffectKind.empty())
    out << ",\"statementKind\":\"" << jsonEscape(node->sourceEffectKind) << '\"';
  if (!node->sourceOriginRole.empty())
    out << ",\"originRole\":\"" << jsonEscape(node->sourceOriginRole) << '\"';
  if (node->sourceMemoryDepth >= 0) out << ",\"storageDepth\":" << node->sourceMemoryDepth;
  if (node->sourceKind == Node::SourceKind::MemoryReadAddress)
    out << ",\"endpoint\":\"" << (node->type == NODE_REG_DST ? "next" : "current") << '\"';
  const auto integers = [&](const char* key, const std::vector<int>& values) {
    out << ",\"" << key << "\":[";
    for (size_t i = 0; i < values.size(); ++i) {
      if (i) out << ',';
      out << values[i];
    }
    out << ']';
  };
  integers("shape", node->semanticShape);
  integers("shapeFieldDepths", node->semanticShapeFieldDepths);
  integers("indices", node->semanticIndices);
  out << ",\"line\":" << (node->sourceLine >= 0 ? node->sourceLine : node->lineno) << '}';
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
    const std::vector<const Node*> nodes = collectGraphNodes(g);
    std::set<std::pair<std::string, std::string>> edges;
    os << "{\n  \"semanticLineageVersion\": 1,\n  \"nodes\": [\n";
    bool firstNode = true;
    for (const Node* node : nodes) {
      if (!firstNode) os << ",\n";
      firstNode = false;
      dumpNode(node);
      for (const Node* next : node->next) edges.insert({node->name, next->name});
    }
    os << "\n  ],\n  \"memoryInventoryComplete\": true,\n  \"memories\": [\n";
    bool firstMemory = true;
    for (const Node* memory : g->memory) {
      if (!firstMemory) os << ",\n";
      firstMemory = false;
      dumpNode(memory);
    }
    os << "\n  ],\n  \"sourceAggregateInventoryComplete\": "
       << (g->sourceAggregateInventoryComplete ? "true" : "false")
       << ",\n  \"sourceAggregates\": [\n";
    bool firstAggregate = true;
    for (const auto& aggregate : g->sourceAggregates) {
      if (!firstAggregate) os << ",\n";
      firstAggregate = false;
      os << "    {\"id\": " << aggregate.id << ", \"name\": \"" << jsonEscape(aggregate.name)
         << "\", \"memberIds\": [";
      for (size_t i = 0; i < aggregate.memberIds.size(); ++i) {
        if (i) os << ", ";
        os << aggregate.memberIds[i];
      }
      os << "], \"originalMemberIds\": [";
      for (size_t i = 0; i < aggregate.originalMemberIds.size(); ++i) {
        if (i) os << ", ";
        os << aggregate.originalMemberIds[i];
      }
      os << "], \"memberFlips\": [";
      for (size_t i = 0; i < aggregate.memberFlips.size(); ++i) {
        if (i) os << ", ";
        os << (aggregate.memberFlips[i] ? "true" : "false");
      }
      os << "], \"nestedAggregateIds\": [";
      for (size_t i = 0; i < aggregate.nestedAggregateIds.size(); ++i) {
        if (i) os << ", ";
        os << aggregate.nestedAggregateIds[i];
      }
      os << "], \"semanticPath\": [";
      for (size_t i = 0; i < aggregate.semanticPath.size(); ++i) {
        if (i) os << ',';
        os << '\"' << jsonEscape(aggregate.semanticPath[i]) << '\"';
      }
      os << "], \"semanticScopeDepth\": " << aggregate.semanticScopeDepth << '}';
    }
    os << "\n  ],\n  \"sourceReplacementInventoryComplete\": "
       << (g->sourceReplacementInventoryComplete ? "true" : "false")
       << ",\n  \"sourceReplacements\": [\n";
    bool firstReplacement = true;
    for (const auto& replacement : g->sourceReplacements) {
      if (!firstReplacement) os << ",\n";
      firstReplacement = false;
      os << "    {\"originalId\": " << replacement.originalId
         << ", \"replacementId\": " << replacement.replacementId
         << ", \"reason\": \"depth-one-memory-to-register\""
         << ", \"originalName\": \"" << jsonEscape(replacement.originalName)
         << "\", \"originalWidth\": " << replacement.originalWidth
         << ", \"originalSign\": " << (replacement.originalSign ? "true" : "false")
         << ", \"originalDimension\": [";
      for (size_t i = 0; i < replacement.originalDimension.size(); ++i) {
        if (i) os << ", ";
        os << replacement.originalDimension[i];
      }
      os << "], \"originalDepth\": " << replacement.originalDepth << "}";
    }
    os << "\n  ],\n  \"sourceScopeInventoryComplete\": "
       << (g->sourceScopeInventoryComplete ? "true" : "false")
       << ",\n  \"sourceScopes\": [\n";
    bool firstScope = true;
    for (const auto& scope : g->sourceScopes) {
      if (!firstScope) os << ",\n";
      firstScope = false;
      os << "    {\"path\": \"" << jsonEscape(scope.path)
         << "\", \"module\": \"" << jsonEscape(scope.module)
         << "\", \"line\": " << scope.line << ", \"instanceLine\": " << scope.instanceLine
         << ", \"kind\": \"" << (scope.external ? "external" : "module") << "\", \"semanticPath\": [";
      for (size_t i = 0; i < scope.semanticPath.size(); ++i) {
        if (i) os << ',';
        os << '\"' << jsonEscape(scope.semanticPath[i]) << '\"';
      }
      os << "]}";
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
  void dumpNode(const Node* node) {
    os << "    {\"id\": " << node->id << ", "
         << "\"name\": \"" << jsonEscape(node->name) << "\", "
         << "\"type\": \"" << nodeTypeToStr(node->type) << "\", "
         << "\"status\": \"" << nodeStatusToStr(node->status) << "\", "
         << "\"width\": " << node->width << ", "
         << "\"sign\": " << node->sign << ", "
         << "\"dimension\": [";
    for (size_t i = 0; i < node->dimension.size(); ++i) {
      if (i) os << ", ";
      os << node->dimension[i];
    }
    os << "], "
         << "\"lineno\": " << node->lineno << ", "
         << "\"super\": " << (node->super ? node->super->id : -1);
    if (node->type == NODE_MEMORY) {
      os << ", \"depth\": " << node->depth
         << ", \"readLatency\": " << node->rlatency
         << ", \"writeLatency\": " << node->wlatency
         << ", \"readUnderWrite\": \"" << jsonEscape(node->extraInfo) << "\""
         << ", \"portIds\": ";
      dumpMemberIds(node);
    }
    if (node->type == NODE_EXT) {
      os << ", \"externalDefname\": \"" << jsonEscape(node->extraInfo) << "\""
         << ", \"memberIds\": ";
      dumpMemberIds(node);
      os << ", \"clockId\": " << (node->clock ? node->clock->id : -1);
      os << ", \"boundaryPortIds\": [";
      bool first = true;
      for (const Node* port : node->externalPorts) {
        if (!port) continue;
        if (!first) os << ", ";
        first = false;
        os << port->id;
      }
      os << "]";
    }
    if (node->parent) os << ", \"parentId\": " << node->parent->id;
    dumpSemantic(os, node);
    if (node->sourceKind != Node::SourceKind::None) {
      os << ", \"sourceKind\": \""
         << (node->sourceKind == Node::SourceKind::WhenCondition ? "when-condition" : "memory-read-address")
         << "\", \"sourceLine\": " << node->sourceLine
         << ", \"sourceOwnerId\": " << node->sourceOwnerId;
    }
    if (node->type == NODE_REG_SRC || node->type == NODE_REG_DST) {
      os << ", \"regNextId\": " << (node->regNext ? node->regNext->id : -1)
         << ", \"regSourceId\": " << (node->type == NODE_REG_SRC ? node->id : node->regNext ? node->regNext->id : -1)
         << ", \"regDestinationId\": " << (node->type == NODE_REG_DST ? node->id : node->regNext ? node->regNext->id : -1);
    }
    if (globalConfig.DumpAssignTree) {
      os << ", \"assignTrees\": ";
      dumpAssignTrees(node);
    }
    os << "}";
  }

  void dumpMemberIds(const Node* node) {
    os << "[";
    bool first = true;
    for (const Node* member : node->member) {
      if (!member) continue;
      if (!first) os << ", ";
      first = false;
      os << member->id;
    }
    os << "]";
  }

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

class ExpressionJsonlDumper {
 public:
  explicit ExpressionJsonlDumper(std::ostream& os) : os(os) {}

  void dump(const graph* g, const std::string& stage) {
    std::vector<const Node*> inventory = collectGraphNodes(g);
    std::sort(inventory.begin(), inventory.end(), [](const Node* a, const Node* b) {
      return a->id < b->id;
    });
    auto findLive = [&](int id) -> const Node* {
      const auto found = std::lower_bound(inventory.begin(), inventory.end(), id,
          [](const Node* node, int value) { return node->id < value; });
      if (found != inventory.end() && (*found)->id == id) return *found;
      for (const Node* memory : g->memory) if (memory->id == id) return memory;
      return nullptr;
    };
    std::set<int> requested;
    if (!globalConfig.ExpressionIdPath.empty()) {
      std::ifstream input(globalConfig.ExpressionIdPath);
      Assert(input.good(), "cannot open expression ID file %s", globalConfig.ExpressionIdPath.c_str());
      std::string line;
      while (std::getline(input, line)) {
        if (line.empty()) continue;
        std::istringstream parser(line);
        int id;
        std::string remainder;
        Assert((parser >> id) && id >= 0 && !(parser >> remainder),
               "invalid expression seed ID %s", line.c_str());
        requested.insert(id);
      }
    }
    std::map<int, const Node*> seeds;
    for (const Node* node : inventory) {
      if (requested.count(node->id) || globalConfig.ExpressionKinds.count(sourceKind(node))) {
        seeds.emplace(node->id, node);
      }
    }
    for (const Node* memory : g->memory) if (requested.count(memory->id)) seeds.emplace(memory->id, memory);
    std::set<int> missingSeeds;
    for (int id : requested) if (!seeds.count(id)) missingSeeds.insert(id);

    struct SeedUse {
      int seedId;
      const char* slot;
      int treeIndex;
      const char* role;
      int expressionId;
    };
    std::map<int, std::vector<SeedUse>> seedUses;
    std::map<int, std::set<int>> consumerIds;
    std::map<int, const Node*> roots = seeds;
    auto inspectConsumer = [&](const Node* node) {
      eachTreeSlot(node, [&](const ExpTree* tree, const char* slot, int index) {
        if (!tree) return;
        auto inspect = [&](const ENode* start, const char* role) {
          std::unordered_set<const ENode*> visited;
          std::vector<const ENode*> pending{start};
          while (!pending.empty()) {
            const ENode* expression = pending.back();
            pending.pop_back();
            if (!expression || !visited.insert(expression).second) continue;
            if (expression->nodePtr && seeds.count(expression->nodePtr->id)) {
              const int seedId = expression->nodePtr->id;
              seedUses[node->id].push_back({seedId, slot, index, role, expression->id});
              consumerIds[seedId].insert(node->id);
              roots.emplace(node->id, node);
            }
            for (const ENode* child : expression->child) pending.push_back(child);
          }
        };
        inspect(tree->getRoot(), "root");
        // The lvalue's owner leaf is a destination, not a value read. Dynamic
        // index children are reads and retain their original expression IDs.
        if (tree->getlval()) for (const ENode* child : tree->getlval()->child) inspect(child, "lvalue-index");
      });
    };
    for (const Node* node : inventory) inspectConsumer(node);
    for (const Node* memory : g->memory) inspectConsumer(memory);
    for (auto current = roots.begin(); current != roots.end(); ++current) {
      const Node* node = current->second;
      if ((node->type == NODE_REG_SRC || node->type == NODE_REG_DST) && node->regNext) {
        roots.emplace(node->regNext->id, node->regNext);
      }
    }
    std::map<int, const Node*> splitSources;
    for (const auto& [original, members] : g->splitArraySources()) splitSources.emplace(original->id, original);
    auto findSource = [&](int id) -> const Node* {
      if (const Node* node = findLive(id)) return node;
      const auto found = splitSources.find(id);
      return found == splitSources.end() ? nullptr : found->second;
    };
    std::map<int, const Node*> selected;
    std::vector<const Node*> work;
    bool capped = false;
    auto add = [&](const Node* node) {
      if (!node) return;
      const auto found = selected.find(node->id);
      if (found != selected.end()) {
        Assert(found->second == node, "expression nodes share physical ID %d", node->id);
        return;
      }
      if (selected.size() >= globalConfig.ExpressionMaxNodes) {
        capped = true;
        return;
      }
      selected.emplace(node->id, node);
      work.push_back(node);
    };
    for (const auto& [id, node] : roots) add(node);
    std::set<int> unresolvedOwners;
    for (size_t i = 0; i < work.size(); ++i) {
      const Node* node = work[i];
      if (globalConfig.ExpressionClosure == "none" ||
          boundaryReason(node, roots.count(node->id))) continue;
      add(node->clock);
      add(node->regNext);
      add(node->parent);
      for (const Node* member : node->member) add(member);
      const auto split = g->splitArraySources().find(const_cast<Node*>(node));
      if (split != g->splitArraySources().end()) for (const Node* member : split->second) add(member);
      if (node->sourceOwnerId >= 0) {
        const Node* owner = findSource(node->sourceOwnerId);
        if (owner) add(owner);
        else unresolvedOwners.insert(node->sourceOwnerId);
      }
      std::unordered_set<const ENode*> visited;
      eachTree(node, [&](const ExpTree* tree) {
        if (!tree) return;
        std::vector<const ENode*> pending{tree->getRoot(), tree->getlval()};
        while (!pending.empty()) {
          const ENode* expression = pending.back();
          pending.pop_back();
          if (!expression || !visited.insert(expression).second) continue;
          add(expression->nodePtr);
          add(expression->memoryNode);
          for (const ENode* child : expression->child) pending.push_back(child);
        }
      });
    }
    os << "{\"record\":\"header\",\"schema\":\"gsim.expressions.v1\",\"graph\":\""
       << jsonEscape(g->name) << "\",\"stage\":\"" << jsonEscape(stage)
       << "\",\"closureMode\":\"" << globalConfig.ExpressionClosure
       << "\",\"maxNodes\":" << globalConfig.ExpressionMaxNodes << ",\"selectedSeedIds\":[";
    bool first = true;
    for (const auto& [id, node] : seeds) {
      if (!first) os << ',';
      first = false;
      os << id;
    }
    os << "],\"verificationRootCount\":" << roots.size() << ",\"requestedMissingSeedIds\":";
    dumpIds(missingSeeds);
    os << ",\"sourceKinds\":[";
    first = true;
    for (const auto& kind : globalConfig.ExpressionKinds) {
      if (!first) os << ',';
      first = false;
      os << '\"' << jsonEscape(kind) << '\"';
    }
    os << "]}\n";
    size_t boundaryCount = 0;
    for (const auto& [id, node] : selected) {
      const char* reason = boundaryReason(node, roots.count(id));
      const bool isBoundary = reason != nullptr;
      boundaryCount += isBoundary;
      const char* location = "dependency-only";
      if (findLive(id) == node) location = node->type == NODE_MEMORY ? "memories" : "nodes";
      dumpNode(g, node, seeds.count(id), roots.count(id), reason, location);
      // Each usage row refers to an actual ENode, not graph adjacency names.
      const auto uses = seedUses.find(id);
      if (uses != seedUses.end()) {
        for (const auto& use : uses->second) {
          os << "{\"record\":\"seed-use\",\"seedId\":" << use.seedId << ",\"consumerId\":" << id
             << ",\"slot\":\"" << use.slot << "\",\"treeIndex\":" << use.treeIndex
             << ",\"role\":\"" << use.role << "\",\"nativeExpressionId\":" << use.expressionId << "}\n";
        }
      }
    }
    const bool complete = !capped && missingSeeds.empty() && unresolvedOwners.empty();
    os << "{\"record\":\"footer\",\"complete\":true,\"closureComplete\":" << (complete ? "true" : "false")
       << ",\"nodeCount\":" << selected.size() << ",\"seedCount\":" << seeds.size()
       << ",\"verificationRootCount\":" << roots.size() << ",\"consumerCount\":" << seedUses.size()
       << ",\"boundaryCount\":" << boundaryCount << ",\"truncationReasons\":[";
    first = true;
    auto reason = [&](const char* text) {
      if (!first) os << ',';
      first = false;
      os << '\"' << text << '\"';
    };
    if (capped) reason("node-cap");
    if (!missingSeeds.empty()) reason("requested-seed-missing");
    if (!unresolvedOwners.empty()) reason("source-owner-unresolved");
    os << "],\"unresolvedSourceOwnerIds\":";
    dumpIds(unresolvedOwners);
    os << "}\n";
    Assert(os.good(), "failed to write expression sidecar");
  }

 private:
  static const char* sourceKind(const Node* node) {
    switch (node->sourceKind) {
      case Node::SourceKind::WhenCondition: return "when-condition";
      case Node::SourceKind::MemoryReadAddress: return "memory-read-address";
      default: return "none";
    }
  }

  static const char* boundaryReason(const Node* node, bool root) {
    if (globalConfig.ExpressionClosure != "combinational" && globalConfig.ExpressionClosure != "source-cuts") return nullptr;
    if (globalConfig.ExpressionClosure == "source-cuts" && !root) {
      if ((node->type == NODE_OTHERS && node->sourceKind == Node::SourceKind::None) ||
          node->type == NODE_REG_SRC || node->type == NODE_MEMORY || node->type == NODE_INP ||
          node->type == NODE_EXT || node->type == NODE_EXT_IN || node->type == NODE_EXT_OUT) {
        return "source-cut-candidate";
      }
    }
    switch (node->type) {
      case NODE_REG_SRC: return root ? nullptr : "state-boundary";
      case NODE_MEMORY: return "memory-boundary";
      case NODE_INP: return "input-boundary";
      case NODE_EXT:
      case NODE_EXT_IN:
      case NODE_EXT_OUT: return "external-boundary";
      case NODE_SPECIAL: return "effect-boundary";
      default: return nullptr;
    }
  }

  template <typename F>
  static void eachTree(const Node* node, F visit) {
    eachTreeSlot(node, [&](const ExpTree* tree, const char*, int) { visit(tree); });
  }

  template <typename F>
  static void eachTreeSlot(const Node* node, F visit) {
    for (size_t i = 0; i < node->assignTree.size(); ++i) visit(node->assignTree[i], "assignTree", static_cast<int>(i));
    visit(node->valTree, "valTree", -1);
    visit(node->resetTree, "resetTree", -1);
    visit(node->resetCond, "resetCond", -1);
    visit(node->resetVal, "resetVal", -1);
    visit(node->memTree, "memTree", -1);
  }

  void pointerId(const Node* node) {
    if (node) os << node->id;
    else os << "null";
  }

  void dumpIds(const std::set<int>& ids) {
    os << '[';
    bool first = true;
    for (int id : ids) {
      if (!first) os << ',';
      first = false;
      os << id;
    }
    os << ']';
  }

  void dumpTree(const ExpTree* tree) {
    if (!tree) {
      os << "null";
      return;
    }
    std::unordered_map<const ENode*, size_t> ids;
    std::vector<const ENode*> expressions;
    auto add = [&](const ENode* expression) {
      if (expression && ids.emplace(expression, expressions.size()).second) expressions.push_back(expression);
    };
    add(tree->getRoot());
    add(tree->getlval());
    for (size_t i = 0; i < expressions.size(); ++i) for (const ENode* child : expressions[i]->child) add(child);
    auto expressionId = [&](const ENode* expression) {
      if (expression) os << ids.at(expression);
      else os << "null";
    };
    os << "{\"root\":";
    expressionId(tree->getRoot());
    os << ",\"lvalue\":";
    expressionId(tree->getlval());
    os << ",\"nodes\":[";
    for (size_t i = 0; i < expressions.size(); ++i) {
      if (i) os << ',';
      const ENode* expression = expressions[i];
      os << "{\"id\":" << i << ",\"nativeId\":" << expression->id
         << ",\"op\":\"" << opTypeToStr(expression->opType) << "\",\"opcode\":" << static_cast<int>(expression->opType)
         << ",\"width\":" << expression->width << ",\"sign\":" << (expression->sign ? "true" : "false")
         << ",\"isClock\":" << (expression->isClock ? "true" : "false")
         << ",\"reset\":" << static_cast<int>(expression->reset) << ",\"usedBit\":" << expression->usedBit
         << ",\"nodeId\":";
      pointerId(expression->nodePtr);
      os << ",\"memoryNodeId\":";
      pointerId(expression->memoryNode);
      os << ",\"values\":[";
      for (size_t j = 0; j < expression->values.size(); ++j) {
        if (j) os << ',';
        os << expression->values[j];
      }
      os << "],\"strVal\":\"" << jsonEscape(expression->strVal) << "\",\"children\":[";
      for (size_t j = 0; j < expression->child.size(); ++j) {
        if (j) os << ',';
        expressionId(expression->child[j]);
      }
      os << "]}";
    }
    os << "]}";
  }

  void dumpNode(const graph* g, const Node* node, bool seed, bool root, const char* reason, const char* location) {
    const bool isBoundary = reason != nullptr;
    const bool treesComplete = globalConfig.ExpressionClosure != "source-cuts" || !isBoundary || root;
    os << "{\"record\":\"node\",\"id\":" << node->id << ",\"name\":\"" << jsonEscape(node->name)
       << "\",\"type\":\"" << nodeTypeToStr(node->type) << "\",\"status\":\"" << nodeStatusToStr(node->status)
       << "\",\"inventory\":\"" << location << "\",\"seed\":" << (seed ? "true" : "false")
       << ",\"verificationRoot\":" << (root ? "true" : "false")
       << ",\"closureBoundary\":" << (isBoundary ? "true" : "false")
       << ",\"boundaryReason\":";
    if (reason) os << '\"' << reason << '\"';
    else os << "null";
    os << ",\"treeInventoryComplete\":" << (treesComplete ? "true" : "false")
       << ",\"width\":" << node->width << ",\"sign\":" << (node->sign ? "true" : "false")
       << ",\"isClock\":" << (node->isClock ? "true" : "false") << ",\"reset\":" << static_cast<int>(node->reset)
       << ",\"asReset\":" << static_cast<int>(node->asReset) << ",\"line\":" << node->lineno
       << ",\"dimension\":[";
    for (size_t i = 0; i < node->dimension.size(); ++i) {
      if (i) os << ',';
      os << node->dimension[i];
    }
    os << "],\"sourceKind\":\"" << sourceKind(node) << "\",\"sourceLine\":" << node->sourceLine
       << ",\"sourceOwnerId\":" << node->sourceOwnerId << ",\"clockId\":";
    pointerId(node->clock);
    os << ",\"regNextId\":";
    pointerId(node->regNext);
    os << ",\"regSourceId\":";
    pointerId(node->type == NODE_REG_SRC ? node : node->type == NODE_REG_DST ? node->regNext : nullptr);
    os << ",\"regDestinationId\":";
    pointerId(node->type == NODE_REG_DST ? node : node->type == NODE_REG_SRC ? node->regNext : nullptr);
    os << ",\"parentId\":";
    pointerId(node->parent);
    os << ",\"memberIds\":[";
    for (size_t i = 0; i < node->member.size(); ++i) {
      if (i) os << ',';
      pointerId(node->member[i]);
    }
    os << "],\"splitMemberIds\":[";
    const auto split = g->splitArraySources().find(const_cast<Node*>(node));
    if (split != g->splitArraySources().end()) {
      for (size_t i = 0; i < split->second.size(); ++i) {
        if (i) os << ',';
        pointerId(split->second[i]);
      }
    }
    os << ']';
    dumpSemantic(os, node);
    if (node->type == NODE_MEMORY) {
      os << ",\"depth\":" << node->depth << ",\"readLatency\":" << node->rlatency
         << ",\"writeLatency\":" << node->wlatency << ",\"readUnderWrite\":\"" << jsonEscape(node->extraInfo) << '\"';
    }
    if (!treesComplete) {
      os << ",\"trees\":null}\n";
      return;
    }
    os << ",\"trees\":{\"assignTree\":[";
    for (size_t i = 0; i < node->assignTree.size(); ++i) {
      if (i) os << ',';
      dumpTree(node->assignTree[i]);
    }
    os << "],\"valTree\":";
    dumpTree(node->valTree);
    os << ",\"resetTree\":";
    dumpTree(node->resetTree);
    os << ",\"resetCond\":";
    dumpTree(node->resetCond);
    os << ",\"resetVal\":";
    dumpTree(node->resetVal);
    os << ",\"memTree\":";
    dumpTree(node->memTree);
    os << "}}\n";
  }

  std::ostream& os;
};

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

    auto collectNodeTree = [&](const ExpTree* tree,
                               std::unordered_set<const ENode*>& nodeVisited) {
      if (!tree) return;
      std::stack<const ENode*> stack;
      if (tree->getRoot()) stack.push(tree->getRoot());
      if (tree->getlval()) stack.push(tree->getlval());
      while (!stack.empty()) {
        const ENode* cur = stack.top();
        stack.pop();
        if (!cur) continue;
        if (!nodeVisited.insert(cur).second) continue;
        for (const ENode* child : cur->child) stack.push(child);
      }
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
  if (globalConfig.DumpExpressions) {
    std::ofstream ofs(prefix + "_Expressions.jsonl");
    ExpressionJsonlDumper(ofs).dump(this, FileName);
  }
}

bool graph::dumpStage(std::string stageName) {
  if (globalConfig.EnableDumpGraph &&
      (globalConfig.DumpStages.empty() || globalConfig.DumpStages.count(stageName))) {
    dump(stageName);
  }
  return !globalConfig.StopAfterStage.empty() && globalConfig.StopAfterStage == stageName;
}
