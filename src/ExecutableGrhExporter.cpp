#include "common.h"
#include "ExecutableGrhEffects.h"
#include "ExecutableGrhExtRegistry.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <unistd.h>

namespace {

constexpr const char* kExecutableGrhFormat = "gsim.executable-grh.v2";

#ifndef GSIM_VERSION
#define GSIM_VERSION "UNKNOWN"
#endif
#ifndef GSIM_BUILD_DATE
#define GSIM_BUILD_DATE "UNKNOWN"
#endif

void writeJsonString(std::ostream& os, const std::string& value) {
  os << '"';
  for (unsigned char c : value) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\b': os << "\\b"; break;
      case '\f': os << "\\f"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        if (c < 0x20) {
          static const char* digits = "0123456789abcdef";
          os << "\\u00" << digits[(c >> 4) & 0xf] << digits[c & 0xf];
        } else {
          os << static_cast<char>(c);
        }
        break;
    }
  }
  os << '"';
}

const char* nodeTypeName(NodeType type) {
  switch (type) {
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
  }
  return "NODE_UNKNOWN";
}

const char* opTypeName(OPType type) {
  switch (type) {
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
  }
  return "OP_UNKNOWN";
}

struct LoweredValue {
  std::string symbol;
  int width = 0;
  bool sign = false;
  int elementWidth = 0;
  bool elementSign = false;
  std::vector<int> dimensions;

  bool isArray() const { return !dimensions.empty(); }
};

enum class JsonAttrKind {
  Bool,
  Int,
  String,
  IntList,
  BoolList,
  StringList,
};

struct JsonAttr {
  std::string key;
  JsonAttrKind kind = JsonAttrKind::String;
  bool boolValue = false;
  int64_t intValue = 0;
  std::string stringValue;
  std::vector<int64_t> intValues;
  std::vector<bool> boolValues;
  std::vector<std::string> stringValues;
};

JsonAttr boolAttr(std::string key, bool value) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::Bool;
  attr.boolValue = value;
  return attr;
}

JsonAttr intAttr(std::string key, int64_t value) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::Int;
  attr.intValue = value;
  return attr;
}

JsonAttr stringAttr(std::string key, std::string value) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::String;
  attr.stringValue = std::move(value);
  return attr;
}

JsonAttr intListAttr(std::string key, std::vector<int64_t> values) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::IntList;
  attr.intValues = std::move(values);
  return attr;
}

JsonAttr boolListAttr(std::string key, std::vector<bool> values) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::BoolList;
  attr.boolValues = std::move(values);
  return attr;
}

JsonAttr stringListAttr(std::string key, std::vector<std::string> values) {
  JsonAttr attr;
  attr.key = std::move(key);
  attr.kind = JsonAttrKind::StringList;
  attr.stringValues = std::move(values);
  return attr;
}

class ExecutableGrhExporter {
 public:
  ExecutableGrhExporter(graph* source, std::string outputPath)
      : source_(source), outputPath_(std::move(outputPath)) {
    const std::string suffix = ".tmp." + std::to_string(static_cast<long long>(getpid()));
    valuesPath_ = outputPath_ + ".values" + suffix;
    opsPath_ = outputPath_ + ".ops" + suffix;
    assembledPath_ = outputPath_ + suffix;
    if (globalConfig.ExecutableGrhProfile == "xiangshan-gsim-coremark-stub") {
      externalProfile_ =
          executable_grh::ExternalModuleExecutionProfile::XiangShanGsimCoremarkStub;
      externalProfileName_ = "xiangshan-gsim-coremark-stub";
    } else if (globalConfig.ExecutableGrhProfile == "full-fidelity") {
      externalProfile_ = executable_grh::ExternalModuleExecutionProfile::FullFidelity;
      externalProfileName_ = "full-fidelity";
    } else {
      externalProfileName_ = globalConfig.ExecutableGrhProfile;
      externalProfileValid_ = false;
    }
  }

  ~ExecutableGrhExporter() {
    std::remove(valuesPath_.c_str());
    std::remove(opsPath_.c_str());
    std::remove(assembledPath_.c_str());
  }

  bool run(std::string& error) {
    if (!prepare()) {
      error = error_;
      return false;
    }
    values_.open(valuesPath_, std::ios::out | std::ios::trunc);
    if (!values_.is_open()) {
      fail("cannot open value spool '" + valuesPath_ + "': " + std::strerror(errno));
      error = error_;
      return false;
    }
    ops_.open(opsPath_, std::ios::out | std::ios::trunc);
    if (!ops_.is_open()) {
      fail("cannot open operation spool '" + opsPath_ + "': " + std::strerror(errno));
      error = error_;
      return false;
    }

    if (!emitModel()) {
      error = error_;
      return false;
    }
    values_.close();
    ops_.close();
    if (!values_ || !ops_) {
      fail("failed while closing executable GRH spool files");
      error = error_;
      return false;
    }
    if (!assemble()) {
      error = error_;
      return false;
    }
    dumpEnodeAttribution();
    error.clear();
    return true;
  }

 private:
  struct ActiveExpressionGuard {
    std::unordered_set<const ENode*>& active;
    const ENode* node;
    ~ActiveExpressionGuard() { active.erase(node); }
  };

  // NO0005 instrumentation: attribute every emitted operation to the enode
  // currently being lowered (innermost context wins), so the export can dump
  // an exact gsim-enode-type x exec-GRH-op-kind count matrix.
  struct EnodeContextGuard {
    std::vector<std::string>& stack;
    EnodeContextGuard(std::vector<std::string>& stack, std::string key)
        : stack(stack) {
      stack.push_back(std::move(key));
    }
    ~EnodeContextGuard() { stack.pop_back(); }
  };

  struct NodeContextGuard {
    std::vector<const Node*>& stack;
    NodeContextGuard(std::vector<const Node*>& stack, const Node* node)
        : stack(stack) {
      stack.push_back(node);
    }
    ~NodeContextGuard() { stack.pop_back(); }
  };

  static std::string effectContextKey(executable_grh::EffectKind kind) {
    switch (kind) {
      case executable_grh::EffectKind::Printf: return "OP_PRINTF";
      case executable_grh::EffectKind::Assert: return "OP_ASSERT";
      case executable_grh::EffectKind::Exit: return "OP_EXIT";
    }
    return "OP_EFFECT_UNKNOWN";
  }

  static std::string enodeContextKey(const ENode* enode) {
    if (!enode) return "<non-enode>";
    if (enode->opType == OP_EMPTY) return enode->nodePtr ? "REF" : "OP_EMPTY";
    static const char* opNames[] = {
        "OP_EMPTY", "OP_MUX", "OP_ADD", "OP_SUB", "OP_MUL", "OP_DIV", "OP_REM",
        "OP_LT", "OP_LEQ", "OP_GT", "OP_GEQ", "OP_EQ", "OP_NEQ", "OP_DSHL",
        "OP_DSHR", "OP_AND", "OP_OR", "OP_XOR", "OP_CAT", "OP_ASUINT",
        "OP_ASSINT", "OP_ASCLOCK", "OP_ASASYNCRESET", "OP_CVT", "OP_NEG",
        "OP_NOT", "OP_ANDR", "OP_ORR", "OP_XORR", "OP_PAD", "OP_SHL", "OP_SHR",
        "OP_HEAD", "OP_TAIL", "OP_BITS", "OP_BITS_NOSHIFT", "OP_INDEX_INT",
        "OP_INDEX", "OP_WHEN", "OP_PRINTF", "OP_ASSERT", "OP_EXIT", "OP_INT",
        "OP_GROUP", "OP_READ_MEM", "OP_WRITE_MEM", "OP_INFER_MEM", "OP_INVALID",
        "OP_RESET", "OP_SEXT", "OP_EXT_FUNC", "OP_STMT_SEQ", "OP_STMT_WHEN",
        "OP_STMT_NODE"};
    const int op = static_cast<int>(enode->opType);
    if (op >= 0 && op < static_cast<int>(sizeof(opNames) / sizeof(opNames[0]))) {
      return opNames[op];
    }
    return "OP_UNKNOWN";
  }

  struct IndexPath {
    LoweredValue selectionShape;
    std::optional<int64_t> staticBitOffset;
    LoweredValue dynamicBitOffset;
    LoweredValue inRange;

    bool isDynamic() const { return !staticBitOffset.has_value(); }
  };

  struct IndexOperand {
    std::optional<int64_t> staticValue;
    LoweredValue dynamicValue;
  };

  struct LoweredMemoryWrite {
    Node* memory = nullptr;
    Node* port = nullptr;
    const ENode* expression = nullptr;
    LoweredValue condition;
    LoweredValue address;
    LoweredValue data;
    LoweredValue mask;
  };

  struct SynchronousMemoryRead {
    Node* memory = nullptr;
    Node* port = nullptr;
    LoweredValue address;
    LoweredValue oldData;
  };

  struct LoweredRegisterUpdate {
    LoweredValue data;
    std::optional<LoweredValue> asyncResetCondition;
  };

  /* one conditional (masked) partial write of a vector register update; the
     per-register list is kept in gsim source order and emitted as consecutive
     kRegisterWritePort ops so the consumer applies them with last-win order */
  struct LoweredRegisterWrite {
    Node* src = nullptr;
    const ENode* expression = nullptr;
    LoweredValue condition;
    LoweredValue data;
    LoweredValue mask;
    bool isSyncReset = false;
    /* async reset leaf only: extra posedge event riding the reset signal */
    std::optional<LoweredValue> asyncEvent;
  };

  struct ExternalInstance {
    Node* root = nullptr;
    executable_grh::ExternalModuleAbi abi;
    executable_grh::ExternalModuleDpiPlan plan;
  };

  struct EmittedValueRecord {
    LoweredValue value;
    bool isInput = false;
    bool isOutput = false;
    const Node* provenance = nullptr;
  };

  struct EmittedOperationRecord {
    std::string symbol;
    std::string kind;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;
    std::vector<JsonAttr> attrs;
  };

  struct NodeEmissionCapture {
    std::vector<EmittedValueRecord> values;
    std::vector<EmittedOperationRecord> operations;
  };

  enum class FinalAssignDisposition {
    Error,
    Elided,
    Keep,
  };

  bool fail(const std::string& message) {
    if (error_.empty()) error_ = message;
    return false;
  }

  std::string nodeContext(const Node* node) const {
    if (!node) return "node <null>";
    return "node id=" + std::to_string(node->id) + " name='" + node->name +
           "' type=" + nodeTypeName(node->type) + " line=" + std::to_string(node->lineno);
  }

  std::string expressionContext(const Node* owner, const ENode* enode) const {
    return nodeContext(owner) + ", expression id=" +
           std::to_string(enode ? enode->id : -1) + " op=" +
           (enode ? opTypeName(enode->opType) : "<null>");
  }

  static LoweredValue scalarValue(std::string symbol, int width, bool sign) {
    LoweredValue value;
    value.symbol = std::move(symbol);
    value.width = width;
    value.sign = sign;
    value.elementWidth = width;
    value.elementSign = sign;
    return value;
  }

  static int bitsRequired(uint64_t maxValue) {
    int bits = 1;
    while (bits < 64 && (maxValue >> bits) != 0) bits++;
    return bits;
  }

  static int64_t entryCount(const std::vector<int>& dimensions) {
    int64_t count = 1;
    for (int dimension : dimensions) count *= dimension;
    return count;
  }

  bool validateNodeShape(const Node* node) {
    int64_t packedWidth = node->width;
    for (size_t i = 0; i < node->dimension.size(); i++) {
      const int dimension = node->dimension[i];
      if (dimension <= 0) {
        return fail(nodeContext(node) + ": array dimension[" + std::to_string(i) +
                    "] must be positive");
      }
      if (packedWidth > std::numeric_limits<int>::max() / dimension) {
        return fail(nodeContext(node) + ": packed array width exceeds native GRH limit");
      }
      packedWidth *= dimension;
    }
    nodePackedWidths_[node] = static_cast<int>(packedWidth);
    return true;
  }

  bool validateCanonicalScalarConstantAssignment(const Node* node,
                                                 const char* role) {
    if (!node || node->status != CONSTANT_NODE || !node->dimension.empty() ||
        node->assignTree.size() != 1 || !node->assignTree[0] ||
        !node->assignTree[0]->getRoot() || !node->assignTree[0]->getlval()) {
      return fail(nodeContext(node) + ": " + role +
                  " is not a canonical scalar constant assignment");
    }
    const ENode* root = node->assignTree[0]->getRoot();
    const ENode* lvalue = node->assignTree[0]->getlval();
    if (root->opType != OP_INT || root->nodePtr || !root->child.empty() ||
        root->width != node->width || root->sign != node->sign ||
        root->strVal.empty() || lvalue->nodePtr != node ||
        !lvalue->child.empty()) {
      return fail(nodeContext(node) + ": " + role +
                  " does not contain one exact-width OP_INT literal");
    }
    return true;
  }

  LoweredValue nodeValue(const Node* node) const {
    LoweredValue value;
    value.symbol = nodeValueSymbol(node);
    value.width = nodePackedWidths_.at(node);
    value.sign = node->dimension.empty() ? node->sign : false;
    value.elementWidth = node->width;
    value.elementSign = node->sign;
    value.dimensions = node->dimension;
    return value;
  }

  static LoweredValue selectionShape(const LoweredValue& packed, size_t indexCount) {
    LoweredValue selected;
    selected.elementWidth = packed.elementWidth;
    selected.elementSign = packed.elementSign;
    selected.dimensions.assign(packed.dimensions.begin() + indexCount,
                               packed.dimensions.end());
    selected.width = selected.elementWidth * static_cast<int>(entryCount(selected.dimensions));
    selected.sign = selected.dimensions.empty() ? selected.elementSign : false;
    return selected;
  }

  static bool samePackedShape(const LoweredValue& lhs, const LoweredValue& rhs) {
    if (lhs.width != rhs.width) return false;
    if (!lhs.isArray() && !rhs.isArray()) return lhs.sign == rhs.sign;
    return lhs.elementWidth == rhs.elementWidth && lhs.elementSign == rhs.elementSign &&
           entryCount(lhs.dimensions) == entryCount(rhs.dimensions);
  }

  bool addSemanticNode(Node* node, const std::string& reason) {
    if (!node) return fail(reason + ": null node");
    auto idIt = nodeById_.find(node->id);
    if (idIt != nodeById_.end() && idIt->second != node) {
      return fail(reason + ": duplicate GSim node id " + std::to_string(node->id));
    }
    nodeById_[node->id] = node;
    if (semanticNodeSet_.insert(node).second) semanticNodes_.push_back(node);
    return true;
  }

  bool validateSupportedNode(const Node* node) {
    if (node->id < 0) return fail(nodeContext(node) + ": invalid negative node id");
    const bool canonicalConstantRegisterDestination =
        node->type == NODE_REG_DST && node->status == CONSTANT_NODE &&
        constantRegisterDestinations_.count(node);
    const bool canonicalConstantOutput =
        node->type == NODE_OUT && node->status == CONSTANT_NODE &&
        constantOutputNodes_.count(node);
    if (node->status != VALID_NODE && !canonicalConstantRegisterDestination &&
        !canonicalConstantOutput) {
      return fail(nodeContext(node) + ": unsupported non-live status " +
                  std::to_string(static_cast<int>(node->status)));
    }
    if (node->type == NODE_EXT) {
      if (!externalInstanceByRoot_.count(node)) {
        return fail(nodeContext(node) + ": missing validated external-module plan");
      }
      return true;
    }
    if (node->width <= 0) return fail(nodeContext(node) + ": width must be positive");
    if (!validateNodeShape(node)) return false;
    switch (node->type) {
      case NODE_INP:
      case NODE_OUT:
      case NODE_OTHERS:
      case NODE_REG_SRC:
      case NODE_REG_DST:
      case NODE_MEMORY:
      case NODE_READER:
      case NODE_WRITER:
      case NODE_READWRITER:
        return true;
      case NODE_INFER:
        return fail(nodeContext(node) +
                    ": unresolved inferred memory port at the executable export boundary");
      case NODE_SPECIAL:
        if (!effectPlans_.count(node) && !optimizerElidedEffects_.count(node)) {
          return fail(nodeContext(node) + ": missing validated executable effect plan");
        }
        return true;
      case NODE_EXT:
        return fail(nodeContext(node) + ": internal external-root validation ordering error");
      case NODE_EXT_IN:
      case NODE_EXT_OUT:
        if (node->type == NODE_EXT_IN && externalClockOwners_.count(node)) {
          if (node->parent) {
            return fail(nodeContext(node) +
                        ": captured external clock must not also be a regular member");
          }
          return true;
        }
        if (!node->parent || node->parent->type != NODE_EXT ||
            !externalInstanceByRoot_.count(node->parent)) {
          return fail(nodeContext(node) + ": external member has no validated owner");
        }
        return true;
      case NODE_REG_RESET:
        return fail(nodeContext(node) +
                    ": synthetic post-coarsen reset nodes are invalid at the pre-coarsen boundary");
      case NODE_INVALID:
        return fail(nodeContext(node) + ": invalid node type");
    }
    return fail(nodeContext(node) + ": unknown node type");
  }

  bool validatePorts(const std::vector<Node*>& ports, NodeType expected,
                     std::unordered_set<const Node*>& portSet, const char* direction) {
    std::unordered_set<std::string> names;
    for (Node* node : ports) {
      if (!node) return fail(std::string(direction) + " port list contains null node");
      if (node->type != expected) {
        return fail(nodeContext(node) + ": listed as top-level " + direction + " port");
      }
      if (!names.insert(node->name).second) {
        return fail("duplicate top-level " + std::string(direction) + " port name '" +
                    node->name + "'");
      }
      if (expected == NODE_OUT && node->status != VALID_NODE) {
        if (!validateCanonicalScalarConstantAssignment(node,
                                                        "constant output port")) {
          return false;
        }
        constantOutputNodes_.insert(node);
      }
      portSet.insert(node);
      if (!addSemanticNode(node, std::string(direction) + " port")) return false;
    }
    return true;
  }

  bool prepareRegisters() {
    for (Node* src : source_->regsrc) {
      if (!src) return fail("register source list contains null node");
      if (src->type != NODE_REG_SRC) {
        return fail(nodeContext(src) + ": graph.regsrc entry is not NODE_REG_SRC");
      }
      if (!src->regSplit) {
        return fail(nodeContext(src) +
                    ": merged register representation is not supported by executable GRH v2");
      }
      Node* dst = src->regNext;
      if (!dst || dst->type != NODE_REG_DST || dst->regNext != src) {
        return fail(nodeContext(src) + ": invalid REG_SRC/REG_DST binding");
      }
      if (src->width != dst->width || src->sign != dst->sign ||
          src->dimension != dst->dimension) {
        return fail(nodeContext(src) + ": register source/destination type mismatch");
      }
      if (!src->clock) return fail(nodeContext(src) + ": register has no clock node");
      if (src->reset == UNCERTAIN) {
        return fail(nodeContext(src) + ": unresolved register reset kind");
      }
      if ((src->reset == UINTRESET || src->reset == ASYRESET) && !src->resetTree) {
        return fail(nodeContext(src) + ": reset register is missing resetTree");
      }
      if (dst->status != VALID_NODE) {
        if (dst->status != CONSTANT_NODE || src->reset != ASYRESET) {
          return fail(nodeContext(dst) +
                      ": unsupported non-live register destination representation");
        }
        if (!validateCanonicalScalarConstantAssignment(
                dst, "constant register destination")) {
          return false;
        }
        constantRegisterDestinations_.insert(dst);
      }
      if (!addSemanticNode(src, "register source") ||
          !addSemanticNode(dst, "register destination") ||
          !addSemanticNode(src->clock, "register clock")) {
        return false;
      }
      registerSources_.push_back(src);
      registerSourceSet_.insert(src);
      registerDestinations_[dst] = src;
    }
    return true;
  }

  bool prepareMemories() {
    std::unordered_set<Node*> seenMemories;
    std::unordered_set<Node*> seenPorts;
    for (Node* memory : source_->memory) {
      if (!memory) return fail("memory list contains null node");
      if (!seenMemories.insert(memory).second) {
        return fail(nodeContext(memory) + ": duplicate backing memory entry");
      }
      if (memory->type != NODE_MEMORY) {
        return fail(nodeContext(memory) + ": graph.memory entry is not NODE_MEMORY");
      }
      if (memory->depth <= 0) {
        return fail(nodeContext(memory) + ": memory depth must be positive");
      }
      if (memory->rlatency != 0 && memory->rlatency != 1) {
        return fail(nodeContext(memory) + ": only read latency 0 or 1 is supported");
      }
      if (memory->wlatency != 1) {
        return fail(nodeContext(memory) + ": only write latency 1 is supported");
      }
      if (!memory->extraInfo.empty() && memory->extraInfo != "undefined" &&
          memory->extraInfo != "old" && memory->extraInfo != "new") {
        return fail(nodeContext(memory) + ": unsupported read-under-write mode '" +
                    memory->extraInfo + "'");
      }
      if (!addSemanticNode(memory, "backing memory")) return false;
      memories_.push_back(memory);

      for (Node* port : memory->member) {
        if (!port) return fail(nodeContext(memory) + ": memory member list contains null port");
        if (!seenPorts.insert(port).second) {
          return fail(nodeContext(port) + ": memory port is attached more than once");
        }
        if (port->parent != memory) {
          return fail(nodeContext(port) + ": memory port parent does not match member owner");
        }
        if (port->type != NODE_READER && port->type != NODE_WRITER &&
            port->type != NODE_READWRITER) {
          return fail(nodeContext(port) + ": unsupported memory member node type");
        }
        if (port->width != memory->width || port->sign != memory->sign ||
            port->dimension != memory->dimension) {
          return fail(nodeContext(port) + ": memory port shape does not match backing memory");
        }
        if (!port->clock) return fail(nodeContext(port) + ": memory port has no clock node");
        if (!port->memTree || !port->memTree->getRoot()) {
          return fail(nodeContext(port) + ": memory port is missing memTree");
        }
        const ENode* root = port->memTree->getRoot();
        // An `infer mport` that is only written is retyped to NODE_WRITER by
        // whenConnect(), while its address-only descriptor intentionally keeps
        // OP_INFER_MEM.  The actual write actions in assignTree use
        // OP_WRITE_MEM.  Explicit write ports use OP_WRITE_MEM in both places.
        const bool validRootOp =
            port->type == NODE_WRITER
                ? (root->opType == OP_WRITE_MEM || root->opType == OP_INFER_MEM)
                : root->opType == OP_READ_MEM;
        if (!validRootOp || root->memoryNode != memory ||
            root->child.size() != 1 || !root->child[0]) {
          return fail(nodeContext(port) + ": malformed memory port memTree (op=" +
                      opTypeName(root->opType) + ", children=" +
                      std::to_string(root->child.size()) + ", memory_matches=" +
                      (root->memoryNode == memory ? "true" : "false") + ")");
        }
        if (!addSemanticNode(port, "memory port") ||
            !addSemanticNode(port->clock, "memory port clock")) {
          return false;
        }
        memoryPorts_.push_back(port);
      }
    }
    return true;
  }

  bool prepareEffects() {
    std::vector<Node*> effects;
    for (Node* node : semanticNodes_) {
      if (node->type == NODE_SPECIAL) effects.push_back(node);
    }
    for (Node* node : effects) {
      executable_grh::EffectPlan plan =
          executable_grh::resolveExecutableGrhEffect(*node);
      if (plan.status == executable_grh::EffectPlanStatus::OptimizerElided) {
        optimizerElidedEffects_.insert(node);
        continue;
      }
      if (plan.status != executable_grh::EffectPlanStatus::Supported) {
        return fail(nodeContext(node) + ": unsupported executable effect: " +
                    (plan.reason.empty() ? "unspecified rejection" : plan.reason));
      }
      if (!plan.baseClock) {
        return fail(nodeContext(node) +
                    ": validated executable effect has no base clock");
      }
      if (!addSemanticNode(const_cast<Node*>(plan.baseClock), "effect base clock")) {
        return false;
      }
      effectPlans_.emplace(node, std::move(plan));
    }
    return true;
  }

  bool validateExternalPlan(const ExternalInstance& instance) {
    const auto& abi = instance.abi;
    const auto& plan = instance.plan;
    std::vector<bool> ignoredParameters(abi.parameters.size(), false);
    std::vector<bool> usedParameters(abi.parameters.size(), false);
    std::vector<bool> ignoredMembers(abi.members.size(), false);
    std::vector<bool> usedMembers(abi.members.size(), false);
    std::vector<size_t> outputWriters(abi.members.size(), 0);

    for (size_t index : plan.ignoredParameters) {
      if (index >= ignoredParameters.size() || ignoredParameters[index]) {
        return fail(nodeContext(instance.root) +
                    ": external plan has invalid/duplicate ignored parameter " +
                    std::to_string(index));
      }
      ignoredParameters[index] = true;
    }
    for (size_t index : plan.ignoredMembers) {
      if (index >= ignoredMembers.size() || ignoredMembers[index]) {
        return fail(nodeContext(instance.root) +
                    ": external plan has invalid/duplicate ignored member " +
                    std::to_string(index));
      }
      if (abi.members[index].direction !=
          executable_grh::ExternalMemberDirection::Input) {
        return fail(nodeContext(instance.root) +
                    ": external output member cannot be ignored");
      }
      ignoredMembers[index] = true;
    }

    for (const auto& constant : plan.outputConstants) {
      if (constant.member >= abi.members.size() || constant.literal.empty()) {
        return fail(nodeContext(instance.root) +
                    ": external plan has malformed output constant");
      }
      if (abi.members[constant.member].direction !=
          executable_grh::ExternalMemberDirection::Output) {
        return fail(nodeContext(instance.root) +
                    ": external constant does not target an output member");
      }
      usedMembers[constant.member] = true;
      outputWriters[constant.member]++;
    }

    for (const auto& call : plan.calls) {
      if (call.importSymbol.empty()) {
        return fail(nodeContext(instance.root) + ": external call has an empty import symbol");
      }
      if (call.eventEdge == executable_grh::DpiEventEdge::None) {
        if (call.useModuleClock) {
          return fail(nodeContext(instance.root) +
                      ": combinational external call unexpectedly requests a clock");
        }
      } else if (!call.useModuleClock || !instance.root->clock) {
        return fail(nodeContext(instance.root) +
                    ": edge-sensitive external call is missing its module clock");
      }
      if (call.hasReturn != (call.returnMember != executable_grh::kNoExternalMember)) {
        return fail(nodeContext(instance.root) +
                    ": external call return/member contract is inconsistent");
      }
      if (!call.hasReturn && !call.inactiveReturnLiteral.empty()) {
        return fail(nodeContext(instance.root) +
                    ": no-return external call has an inactive return literal");
      }
      if (call.hasReturn) {
        if (call.returnMember >= abi.members.size() || call.returnWidth <= 0 ||
            call.returnType.empty()) {
          return fail(nodeContext(instance.root) +
                      ": external return plan is malformed");
        }
        if (abi.members[call.returnMember].direction !=
            executable_grh::ExternalMemberDirection::Output) {
          return fail(nodeContext(instance.root) +
                      ": external return does not target an output member");
        }
        usedMembers[call.returnMember] = true;
        outputWriters[call.returnMember]++;
      }

      std::unordered_set<std::string> argumentNames;
      for (const auto& argument : call.arguments) {
        if (argument.name.empty() || argument.width <= 0 || argument.type.empty() ||
            !argumentNames.insert(argument.name).second) {
          return fail(nodeContext(instance.root) +
                      ": external call has a malformed/duplicate argument");
        }
        switch (argument.source.kind) {
          case executable_grh::DpiValueSourceKind::Member: {
            if (argument.source.index >= abi.members.size()) {
              return fail(nodeContext(instance.root) +
                          ": external argument references an invalid member");
            }
            const auto& member = abi.members[argument.source.index];
            size_t elements = 1;
            for (int64_t dimension : member.dimensions) {
              if (dimension <= 0 ||
                  elements > std::numeric_limits<size_t>::max() /
                                 static_cast<size_t>(dimension)) {
                return fail(nodeContext(instance.root) +
                            ": external argument member has an invalid array shape");
              }
              elements *= static_cast<size_t>(dimension);
            }
            if (argument.source.element >= elements) {
              return fail(nodeContext(instance.root) +
                          ": external argument references an invalid array element");
            }
            const bool input = argument.direction == executable_grh::DpiArgDirection::Input;
            const auto expected = input
                ? executable_grh::ExternalMemberDirection::Input
                : executable_grh::ExternalMemberDirection::Output;
            if (member.direction != expected) {
              return fail(nodeContext(instance.root) +
                          ": external argument/member directions disagree");
            }
            usedMembers[argument.source.index] = true;
            if (!input) outputWriters[argument.source.index]++;
            break;
          }
          case executable_grh::DpiValueSourceKind::Parameter:
            if (argument.direction != executable_grh::DpiArgDirection::Input ||
                argument.source.index >= abi.parameters.size()) {
              return fail(nodeContext(instance.root) +
                          ": external argument references an invalid parameter");
            }
            usedParameters[argument.source.index] = true;
            break;
          case executable_grh::DpiValueSourceKind::Literal:
            if (argument.direction != executable_grh::DpiArgDirection::Input ||
                argument.source.literal.empty()) {
              return fail(nodeContext(instance.root) +
                          ": external literal argument is malformed");
            }
            break;
        }
      }

      for (size_t memberIndex : call.conditionMembers) {
        if (memberIndex >= abi.members.size()) {
          return fail(nodeContext(instance.root) +
                      ": external call condition references an invalid member");
        }
        const auto& member = abi.members[memberIndex];
        if (member.direction != executable_grh::ExternalMemberDirection::Input ||
            member.width != 1 || !member.dimensions.empty()) {
          return fail(nodeContext(instance.root) +
                      ": external call condition must reference a scalar one-bit input");
        }
        usedMembers[memberIndex] = true;
      }
    }

    for (size_t i = 0; i < abi.parameters.size(); ++i) {
      if (ignoredParameters[i] && usedParameters[i]) {
        return fail(nodeContext(instance.root) +
                    ": external parameter is both used and ignored");
      }
      if (!ignoredParameters[i] && !usedParameters[i]) {
        return fail(nodeContext(instance.root) +
                    ": external parameter is not consumed by its plan");
      }
    }
    for (size_t i = 0; i < abi.members.size(); ++i) {
      if (ignoredMembers[i] && usedMembers[i]) {
        return fail(nodeContext(instance.root) +
                    ": external member is both used and ignored");
      }
      if (abi.members[i].direction == executable_grh::ExternalMemberDirection::Output) {
        if (outputWriters[i] != 1) {
          return fail(nodeContext(instance.root) + ": external output member[" +
                      std::to_string(i) + "] must have exactly one writer");
        }
      } else if (!ignoredMembers[i] && !usedMembers[i]) {
        return fail(nodeContext(instance.root) + ": external input member[" +
                    std::to_string(i) + "] is not consumed by its plan");
      }
    }
    return true;
  }

  bool prepareExternalModules() {
    std::vector<Node*> roots;
    for (Node* node : semanticNodes_) {
      if (node->type == NODE_EXT) roots.push_back(node);
    }
    for (Node* root : roots) {
      ExternalInstance instance;
      instance.root = root;
      instance.abi.defname = root->extraInfo.empty() ? root->name : root->extraInfo;
      instance.abi.hasClock = root->clock != nullptr;
      if (root->clock) {
        if (root->clock->type != NODE_EXT_IN || !root->clock->isClock ||
            root->clock->width != 1 || root->clock->sign ||
            !root->clock->dimension.empty()) {
          return fail(nodeContext(root) +
                      ": captured external clock must be an unsigned scalar NODE_EXT_IN Clock");
        }
        auto [owner, inserted] = externalClockOwners_.emplace(root->clock, root);
        if (!inserted && owner->second != root) {
          return fail(nodeContext(root->clock) +
                      ": captured external clock is shared by multiple external roots");
        }
        instance.abi.clockName = root->clock->name;
        if (!addSemanticNode(root->clock, "external module clock")) return false;
      }
      for (const auto& parameter : root->params) {
        executable_grh::ExternalParamAbi abi;
        abi.kind = parameter.first ? executable_grh::ExternalParamKind::Integer
                                   : executable_grh::ExternalParamKind::String;
        abi.value = parameter.second;
        instance.abi.parameters.push_back(std::move(abi));
      }

      std::vector<Node*> inputMembers;
      for (Node* member : root->member) {
        if (!member || member->parent != root ||
            member == root->clock ||
            (member->type != NODE_EXT_IN && member->type != NODE_EXT_OUT)) {
          return fail(nodeContext(root) + ": malformed external member list");
        }
        if (!addSemanticNode(member, "external module member")) return false;
        executable_grh::ExternalMemberAbi abi;
        abi.name = member->name;
        abi.direction = member->type == NODE_EXT_IN
            ? executable_grh::ExternalMemberDirection::Input
            : executable_grh::ExternalMemberDirection::Output;
        abi.width = member->width;
        abi.isSigned = member->sign;
        for (int dimension : member->dimension) abi.dimensions.push_back(dimension);
        instance.abi.members.push_back(std::move(abi));
        if (member->type == NODE_EXT_IN) inputMembers.push_back(member);
      }

      if (root->assignTree.size() != 1 ||
          !validateLvalue(root, root->assignTree.front(), 0)) {
        return fail(nodeContext(root) + ": external root must have one call tree");
      }
      const ENode* callRoot = root->assignTree.front()->getRoot();
      if (callRoot->opType != OP_EXT_FUNC ||
          callRoot->child.size() != inputMembers.size()) {
        return fail(nodeContext(root) + ": malformed external root OP_EXT_FUNC");
      }
      for (size_t i = 0; i < inputMembers.size(); ++i) {
        if (!callRoot->child[i] || callRoot->child[i]->nodePtr != inputMembers[i] ||
            !callRoot->child[i]->child.empty()) {
          return fail(nodeContext(root) +
                      ": external root input order does not match its member ABI");
        }
      }
      for (Node* member : root->member) {
        if (member->type != NODE_EXT_OUT) continue;
        if (member->assignTree.size() != 1 ||
            !validateLvalue(member, member->assignTree.front(), 0)) {
          return fail(nodeContext(member) + ": external output must have one call tree");
        }
        const ENode* outputCall = member->assignTree.front()->getRoot();
        while (outputCall && outputCall->opType != OP_EXT_FUNC) {
          const bool canonicalCast =
              outputCall->opType == OP_PAD || outputCall->opType == OP_SEXT ||
              outputCall->opType == OP_ASUINT || outputCall->opType == OP_ASSINT ||
              outputCall->opType == OP_CVT;
          if (!canonicalCast || outputCall->nodePtr ||
              outputCall->child.size() != 1 || !outputCall->child[0]) {
            outputCall = nullptr;
            break;
          }
          outputCall = outputCall->child[0];
        }
        if (!outputCall || outputCall->child.size() != 1 ||
            !outputCall->child[0] || outputCall->child[0]->nodePtr != root ||
            !outputCall->child[0]->child.empty()) {
          return fail(nodeContext(member) + ": malformed external output OP_EXT_FUNC");
        }
      }

      instance.plan = executable_grh::resolveKnownXiangShanExternalModule(
          instance.abi, externalProfile_);
      if (instance.plan.status == executable_grh::ExternalModulePlanStatus::RequiresWrapper) {
        return fail(nodeContext(root) + ": external defname '" + instance.abi.defname +
                    "' requires wrapper '" + instance.plan.requiredWrapper + "': " +
                    instance.plan.reason);
      }
      if (instance.plan.status != executable_grh::ExternalModulePlanStatus::Supported) {
        return fail(nodeContext(root) + ": unsupported external defname '" +
                    instance.abi.defname + "': " + instance.plan.reason);
      }
      externalInstances_.push_back(std::move(instance));
      externalInstanceByRoot_[root] = externalInstances_.size() - 1;
      if (!validateExternalPlan(externalInstances_.back())) return false;
    }
    return true;
  }

  bool prepare() {
    if (!source_) return fail("cannot export a null GSim graph");
    if (outputPath_.empty()) return fail("executable GRH output path is empty");
    if (!externalProfileValid_) {
      return fail("unsupported executable GRH profile '" + externalProfileName_ + "'");
    }
    if (source_->name.empty()) return fail("GSim graph has an empty top name");

    for (SuperNode* super : source_->sortedSuper) {
      if (!super) return fail("sortedSuper contains a null supernode");
      for (Node* node : super->member) {
        if (!addSemanticNode(node, "sorted pre-coarsen member")) return false;
      }
    }
    if (!validatePorts(source_->input, NODE_INP, inputNodes_, "input")) return false;
    if (!validatePorts(source_->output, NODE_OUT, outputNodes_, "output")) return false;
    if (!prepareExternalModules()) return false;
    if (!prepareMemories()) return false;
    if (!prepareRegisters()) return false;
    if (!prepareEffects()) return false;

    for (Node* node : semanticNodes_) {
      if (!validateSupportedNode(node)) return false;
      if (node->type == NODE_REG_SRC && !registerSourceSet_.count(node)) {
        return fail(nodeContext(node) + ": live register source is absent from graph.regsrc");
      }
      if (node->type == NODE_REG_DST && !registerDestinations_.count(node)) {
        return fail(nodeContext(node) + ": live register destination has no source binding");
      }
    }
    return true;
  }

  std::string nodeValueSymbol(const Node* node) const {
    return "gsim.v." + std::to_string(node->id);
  }

  std::string registerSymbol(const Node* source) const {
    return "gsim.reg." + std::to_string(source->id);
  }

  std::string memorySymbol(const Node* memory) const {
    return "gsim.mem." + std::to_string(memory->id);
  }

  std::string memoryReadRegisterSymbol(const Node* port) const {
    return "gsim.mem_read_reg." + std::to_string(port->id);
  }

  std::string nextTemporaryValueSymbol() {
    return "gsim.tmp." + std::to_string(nextValueId_++);
  }

  std::string nextExpressionOpSymbol() {
    return "gsim.expr." + std::to_string(nextOperationId_++);
  }

  void writeAttrs(std::ostream& os, const std::vector<JsonAttr>& attrs) {
    os << "{";
    for (size_t i = 0; i < attrs.size(); i++) {
      if (i) os << ", ";
      writeJsonString(os, attrs[i].key);
      os << ": {";
      switch (attrs[i].kind) {
        case JsonAttrKind::Bool:
          os << "\"t\": \"bool\", \"v\": " <<
                (attrs[i].boolValue ? "true" : "false");
          break;
        case JsonAttrKind::Int:
          os << "\"t\": \"int\", \"v\": " << attrs[i].intValue;
          break;
        case JsonAttrKind::String:
          os << "\"t\": \"string\", \"v\": ";
          writeJsonString(os, attrs[i].stringValue);
          break;
        case JsonAttrKind::IntList:
          os << "\"t\": \"int[]\", \"vs\": [";
          for (size_t j = 0; j < attrs[i].intValues.size(); j++) {
            if (j) os << ", ";
            os << attrs[i].intValues[j];
          }
          os << "]";
          break;
        case JsonAttrKind::BoolList:
          os << "\"t\": \"bool[]\", \"vs\": [";
          for (size_t j = 0; j < attrs[i].boolValues.size(); j++) {
            if (j) os << ", ";
            os << (attrs[i].boolValues[j] ? "true" : "false");
          }
          os << "]";
          break;
        case JsonAttrKind::StringList:
          os << "\"t\": \"string[]\", \"vs\": [";
          for (size_t j = 0; j < attrs[i].stringValues.size(); j++) {
            if (j) os << ", ";
            writeJsonString(os, attrs[i].stringValues[j]);
          }
          os << "]";
          break;
      }
      os << "}";
    }
    os << "}";
  }

  bool writeValueRecord(const EmittedValueRecord& record) {
    const LoweredValue& value = record.value;
    if (!firstValue_) values_ << ",\n";
    firstValue_ = false;
    values_ << "        {\"sym\": ";
    writeJsonString(values_, value.symbol);
    values_ << ", \"w\": " << value.width
            << ", \"sgn\": " << (value.sign ? "true" : "false")
            << ", \"type\": \"logic\", \"in\": "
            << (record.isInput ? "true" : "false")
            << ", \"out\": " << (record.isOutput ? "true" : "false")
            << ", \"inout\": false";
    if (record.provenance) {
      const Node* provenance = record.provenance;
      std::vector<JsonAttr> attrs = {
          intAttr("gsim.node_id", provenance->id),
          stringAttr("gsim.node_name", provenance->name),
          stringAttr("gsim.node_type", nodeTypeName(provenance->type)),
          intAttr("gsim.source_line", provenance->lineno),
      };
      if (!provenance->dimension.empty()) {
        std::vector<int64_t> dimensions;
        dimensions.reserve(provenance->dimension.size());
        for (int dimension : provenance->dimension) dimensions.push_back(dimension);
        attrs.push_back(intAttr("gsim.element_width", provenance->width));
        attrs.push_back(boolAttr("gsim.element_signed", provenance->sign));
        attrs.push_back(intListAttr("gsim.dimensions", std::move(dimensions)));
        attrs.push_back(stringAttr("gsim.packed_order", "element0-lsb"));
      }
      values_ << ", \"attrs\": ";
      writeAttrs(values_, attrs);
    }
    values_ << "}";
    return static_cast<bool>(values_);
  }

  bool emitValue(const LoweredValue& value, bool isInput = false, bool isOutput = false,
                 const Node* provenance = nullptr) {
    if (value.width <= 0) return fail("attempted to emit non-positive-width value '" + value.symbol + "'");
    if (!emittedValueSymbols_.insert(value.symbol).second) {
      return fail("duplicate executable GRH value symbol '" + value.symbol + "'");
    }
    EmittedValueRecord record{value, isInput, isOutput, provenance};
    if (nodeEmissionCapture_) {
      nodeEmissionCapture_->values.push_back(std::move(record));
      return true;
    }
    return writeValueRecord(record);
  }

  bool writeOperationRecord(const EmittedOperationRecord& record) {
    if (!firstOperation_) ops_ << ",\n";
    firstOperation_ = false;
    ops_ << "        {\"sym\": ";
    writeJsonString(ops_, record.symbol);
    ops_ << ", \"kind\": ";
    writeJsonString(ops_, record.kind);
    ops_ << ", \"in\": [";
    for (size_t i = 0; i < record.inputs.size(); i++) {
      if (i) ops_ << ", ";
      writeJsonString(ops_, record.inputs[i]);
    }
    ops_ << "], \"out\": [";
    for (size_t i = 0; i < record.outputs.size(); i++) {
      if (i) ops_ << ", ";
      writeJsonString(ops_, record.outputs[i]);
    }
    ops_ << "]";
    if (!record.attrs.empty()) {
      ops_ << ", \"attrs\": ";
      writeAttrs(ops_, record.attrs);
    }
    ops_ << "}";
    return static_cast<bool>(ops_);
  }

  static bool hasAttrKey(const std::vector<JsonAttr>& attrs, const std::string& key) {
    for (const JsonAttr& attr : attrs) {
      if (attr.key == key) return true;
    }
    return false;
  }

  bool emitOperation(const std::string& symbol, const std::string& kind,
                     const std::vector<std::string>& inputs,
                     const std::vector<std::string>& outputs,
                     const std::vector<JsonAttr>& attrs = {}) {
    if (!emittedOperationSymbols_.insert(symbol).second) {
      return fail("duplicate executable GRH operation symbol '" + symbol + "'");
    }
    enodeOpAttribution_[enodeContextStack_.empty() ? std::string("<non-enode>")
                                                   : enodeContextStack_.back()][kind]++;
    EmittedOperationRecord record{symbol, kind, inputs, outputs, attrs};
    // Stamp every node-owned operation with its owner node id so downstream
    // consumers can rebuild gsim node groupings without heuristics.  Global
    // constants stay unowned (shared across nodes by design); operations that
    // already carry an explicit gsim.node_id keep it.
    if (kind != "kConstant" && !hasAttrKey(record.attrs, "gsim.node_id")) {
      if (!nodeContextStack_.empty()) {
        record.attrs.push_back(intAttr("gsim.node_id", nodeContextStack_.back()->id));
      } else {
        unownedOpCounts_[kind]++;
      }
    }
    if (nodeEmissionCapture_) {
      nodeEmissionCapture_->operations.push_back(std::move(record));
      return true;
    }
    return writeOperationRecord(record);
  }

  bool flushNodeEmissionCapture() {
    if (!nodeEmissionCapture_) return fail("no executable GRH node emission capture to flush");
    for (const EmittedValueRecord& record : nodeEmissionCapture_->values) {
      if (!writeValueRecord(record)) return false;
    }
    for (const EmittedOperationRecord& record : nodeEmissionCapture_->operations) {
      if (!writeOperationRecord(record)) return false;
    }
    nodeEmissionCapture_.reset();
    return true;
  }

  FinalAssignDisposition finishNodeEmissionCapture(
      const LoweredValue& current, const LoweredValue& target,
      const std::vector<JsonAttr>& nodeAttrs) {
    if (!nodeEmissionCapture_) {
      fail("no executable GRH node emission capture to finish");
      return FinalAssignDisposition::Error;
    }

    size_t valueIndex = nodeEmissionCapture_->values.size();
    size_t producerIndex = nodeEmissionCapture_->operations.size();
    size_t valueMatches = 0;
    size_t producerMatches = 0;
    size_t inputUses = 0;
    for (size_t i = 0; i < nodeEmissionCapture_->values.size(); i++) {
      if (nodeEmissionCapture_->values[i].value.symbol == current.symbol) {
        valueIndex = i;
        valueMatches++;
      }
    }
    for (size_t i = 0; i < nodeEmissionCapture_->operations.size(); i++) {
      const EmittedOperationRecord& operation = nodeEmissionCapture_->operations[i];
      inputUses += static_cast<size_t>(
          std::count(operation.inputs.begin(), operation.inputs.end(), current.symbol));
      const size_t outputMatches = static_cast<size_t>(
          std::count(operation.outputs.begin(), operation.outputs.end(), current.symbol));
      if (outputMatches != 0) {
        producerIndex = i;
        producerMatches += outputMatches;
      }
    }

    const bool temporary = current.symbol.compare(0, 9, "gsim.tmp.") == 0;
    bool canRetarget = temporary && valueMatches == 1 && producerMatches == 1 &&
                       inputUses == 0 && producerIndex < nodeEmissionCapture_->operations.size();
    if (canRetarget) {
      const EmittedOperationRecord& producer =
          nodeEmissionCapture_->operations[producerIndex];
      canRetarget = producer.kind != "kConstant" && producer.outputs.size() == 1;
    }

    if (canRetarget) {
      EmittedOperationRecord& producer = nodeEmissionCapture_->operations[producerIndex];
      producer.outputs[0] = target.symbol;
      // The auto-stamped owner id duplicates the anchor attr; drop it so the
      // merged attr list carries each key exactly once.
      producer.attrs.erase(
          std::remove_if(producer.attrs.begin(), producer.attrs.end(),
                         [](const JsonAttr& attr) {
                           return attr.key == "gsim.node_id" ||
                                  attr.key == "gsim.node_name";
                         }),
          producer.attrs.end());
      producer.attrs.insert(producer.attrs.end(), nodeAttrs.begin(), nodeAttrs.end());
      nodeEmissionCapture_->values.erase(nodeEmissionCapture_->values.begin() + valueIndex);
      emittedValueSymbols_.erase(current.symbol);
      nodeFinalAssignElidedCount_++;
      if (!flushNodeEmissionCapture()) return FinalAssignDisposition::Error;
      return FinalAssignDisposition::Elided;
    }

    nodeFinalAssignKeptCount_++;
    if (!flushNodeEmissionCapture()) return FinalAssignDisposition::Error;
    return FinalAssignDisposition::Keep;
  }

  std::optional<std::string> canonicalFirrtlLiteral(const ENode* enode, const Node* owner) {
    if (!enode || enode->width <= 0) {
      fail(expressionContext(owner, enode) + ": integer literal has invalid width");
      return std::nullopt;
    }
    std::string text = enode->strVal.empty() ? "0" : enode->strVal;
    bool negative = false;
    size_t cursor = 0;
    if (text[cursor] == '-') {
      negative = true;
      cursor++;
    }
    int base = 10;
    if (cursor + 1 < text.size() && text[cursor] == '0') {
      switch (text[cursor + 1]) {
        case 'b': base = 2; cursor += 2; break;
        case 'o': base = 8; cursor += 2; break;
        case 'd': base = 10; cursor += 2; break;
        case 'h': base = 16; cursor += 2; break;
        default: break;
      }
    }
    std::string digits = text.substr(cursor);
    if (digits.empty()) digits = "0";
    if (negative) digits.insert(digits.begin(), '-');

    mpz_t value;
    mpz_init(value);
    if (mpz_set_str(value, digits.c_str(), base) != 0) {
      mpz_clear(value);
      fail(expressionContext(owner, enode) + ": invalid FIRRTL integer literal '" + text + "'");
      return std::nullopt;
    }
    mpz_fdiv_r_2exp(value, value, static_cast<mp_bitcnt_t>(enode->width));
    const size_t chars = mpz_sizeinbase(value, 16) + 2;
    std::vector<char> buffer(chars, '\0');
    mpz_get_str(buffer.data(), 16, value);
    mpz_clear(value);
    return std::to_string(enode->width) + "'h" + std::string(buffer.data());
  }

  LoweredValue emitConstant(int width, bool sign, const std::string& literal) {
    const std::string key = std::to_string(width) + ":" + (sign ? "s:" : "u:") + literal;
    auto cached = constantValues_.find(key);
    if (cached != constantValues_.end()) return cached->second;
    LoweredValue result = scalarValue(nextTemporaryValueSymbol(), width, sign);
    if (!emitValue(result) ||
        !emitOperation(nextExpressionOpSymbol(), "kConstant", {}, {result.symbol},
                       {stringAttr("constValue", literal)})) {
      return {};
    }
    constantValues_.emplace(key, result);
    return result;
  }

  std::string emitStringConstant(const std::string& literal) {
    auto cached = stringConstantValues_.find(literal);
    if (cached != stringConstantValues_.end()) return cached->second;

    const std::string symbol = nextTemporaryValueSymbol();
    if (!emittedValueSymbols_.insert(symbol).second) {
      fail("duplicate executable GRH value symbol '" + symbol + "'");
      return {};
    }
    if (!firstValue_) values_ << ",\n";
    firstValue_ = false;
    values_ << "        {\"sym\": ";
    writeJsonString(values_, symbol);
    values_ << ", \"w\": 0, \"sgn\": false, \"type\": \"string\", "
               "\"in\": false, \"out\": false, \"inout\": false}";
    if (!values_) {
      fail("failed writing executable GRH string value '" + symbol + "'");
      return {};
    }
    if (!emitOperation(nextExpressionOpSymbol(), "kConstant", {}, {symbol},
                       {stringAttr("constValue", literal)})) {
      return {};
    }
    stringConstantValues_.emplace(literal, symbol);
    return symbol;
  }

  LoweredValue emitUnsignedDecimalConstant(int width, uint64_t value) {
    return emitConstant(width, false,
                        std::to_string(width) + "'d" + std::to_string(value));
  }

  LoweredValue emitAllOnesConstant(int width) {
    mpz_t value;
    mpz_init_set_ui(value, 1);
    mpz_mul_2exp(value, value, static_cast<mp_bitcnt_t>(width));
    mpz_sub_ui(value, value, 1);
    const size_t chars = mpz_sizeinbase(value, 16) + 2;
    std::vector<char> buffer(chars, '\0');
    mpz_get_str(buffer.data(), 16, value);
    mpz_clear(value);
    return emitConstant(width, false,
                        std::to_string(width) + "'h" + std::string(buffer.data()));
  }

  LoweredValue emitRangeOnesConstant(int width, int low, int high) {
    if (width <= 0 || low < 0 || high < low || high >= width) {
      fail("invalid range mask [" + std::to_string(high) + ":" +
           std::to_string(low) + "] for width " + std::to_string(width));
      return {};
    }
    mpz_t value;
    mpz_init_set_ui(value, 1);
    mpz_mul_2exp(value, value, static_cast<mp_bitcnt_t>(high - low + 1));
    mpz_sub_ui(value, value, 1);
    mpz_mul_2exp(value, value, static_cast<mp_bitcnt_t>(low));
    const size_t chars = mpz_sizeinbase(value, 16) + 2;
    std::vector<char> buffer(chars, '\0');
    mpz_get_str(buffer.data(), 16, value);
    mpz_clear(value);
    return emitConstant(width, false,
                        std::to_string(width) + "'h" + std::string(buffer.data()));
  }

  LoweredValue emitStaticSlice(const Node* owner, const ENode* enode,
                               const LoweredValue& source, int64_t start,
                               LoweredValue result) {
    if (start < 0 || result.width <= 0 ||
        start > static_cast<int64_t>(source.width) - result.width) {
      fail(expressionContext(owner, enode) + ": static slice [" +
           std::to_string(start + result.width - 1) + ":" +
           std::to_string(start) + "] exceeds source width " +
           std::to_string(source.width));
      return {};
    }
    return emitTypedOperation(
        "kSliceStatic", {source}, std::move(result),
        {intAttr("sliceStart", start),
         intAttr("sliceEnd", start + result.width - 1)});
  }

  LoweredValue emitDynamicSlice(const LoweredValue& source,
                                const LoweredValue& bitOffset,
                                LoweredValue result) {
    const int width = result.width;
    return emitTypedOperation("kSliceDynamic", {source, bitOffset},
                              std::move(result),
                              {intAttr("sliceWidth", width)});
  }

  LoweredValue emitConcat(const Node* owner, const ENode* enode,
                          const std::vector<LoweredValue>& highToLow,
                          LoweredValue result) {
    int64_t width = 0;
    for (const LoweredValue& operand : highToLow) width += operand.width;
    if (highToLow.empty() || width != result.width) {
      fail(expressionContext(owner, enode) + ": concat operand width " +
           std::to_string(width) + " does not match result width " +
           std::to_string(result.width));
      return {};
    }
    if (highToLow.size() == 1) {
      result.symbol = highToLow.front().symbol;
      return result;
    }
    return emitTypedOperation("kConcat", highToLow, std::move(result));
  }

  std::optional<LoweredValue> coerceToShape(const Node* owner, const ENode* enode,
                                            const LoweredValue& source,
                                            LoweredValue target) {
    if (target.width <= 0 || target.elementWidth <= 0) {
      fail(expressionContext(owner, enode) + ": invalid assignment target shape");
      return std::nullopt;
    }
    if (target.isArray() && !source.isArray()) {
      LoweredValue elementTarget = scalarValue({}, target.elementWidth, target.elementSign);
      auto element = coerceToShape(owner, enode, source, elementTarget);
      if (!element) return std::nullopt;
      const int64_t count = entryCount(target.dimensions);
      std::vector<LoweredValue> operands(static_cast<size_t>(count), *element);
      LoweredValue result = emitConcat(owner, enode, operands, target);
      if (result.symbol.empty()) return std::nullopt;
      return result;
    }
    if (source.isArray() && target.isArray()) {
      const int64_t sourceCount = entryCount(source.dimensions);
      const int64_t targetCount = entryCount(target.dimensions);
      if (sourceCount != targetCount) {
        fail(expressionContext(owner, enode) +
             ": incompatible packed array assignment shape");
        return std::nullopt;
      }
      if (source.elementWidth != target.elementWidth ||
          source.elementSign != target.elementSign) {
        std::vector<LoweredValue> highToLow;
        highToLow.reserve(static_cast<size_t>(sourceCount));
        for (int64_t index = sourceCount; index-- > 0;) {
          LoweredValue sourceElement = emitStaticSlice(
              owner, enode, source, index * source.elementWidth,
              scalarValue({}, source.elementWidth, source.elementSign));
          if (sourceElement.symbol.empty()) return std::nullopt;
          auto targetElement = coerceToShape(
              owner, enode, sourceElement,
              scalarValue({}, target.elementWidth, target.elementSign));
          if (!targetElement) return std::nullopt;
          highToLow.push_back(*targetElement);
        }
        LoweredValue result = emitConcat(owner, enode, highToLow, target);
        if (result.symbol.empty()) return std::nullopt;
        return result;
      }
    } else if (source.isArray() && source.width != target.width) {
      fail(expressionContext(owner, enode) +
           ": packed array cannot be coerced to a different scalar width");
      return std::nullopt;
    }

    if (source.width == target.width && source.sign == target.sign) {
      target.symbol = source.symbol;
      return target;
    }
    LoweredValue result = emitTypedOperation("kAssign", {source}, target);
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  LoweredValue emitTypedOperation(const std::string& kind,
                                  const std::vector<LoweredValue>& operands,
                                  LoweredValue result,
                                  const std::vector<JsonAttr>& attrs = {}) {
    result.symbol = nextTemporaryValueSymbol();
    if (!emitValue(result)) return {};
    std::vector<std::string> inputs;
    inputs.reserve(operands.size());
    for (const LoweredValue& operand : operands) inputs.push_back(operand.symbol);
    if (!emitOperation(nextExpressionOpSymbol(), kind, inputs, {result.symbol}, attrs)) return {};
    return result;
  }

  LoweredValue emitExpressionOperation(const ENode* enode, const std::string& kind,
                                       const std::vector<LoweredValue>& operands,
                                       const std::vector<JsonAttr>& attrs = {}) {
    return emitTypedOperation(kind, operands,
                              scalarValue({}, enode->width, enode->sign), attrs);
  }

  bool expectChildren(const Node* owner, const ENode* enode, size_t count, bool allowNull) {
    if (enode->child.size() != count) {
      return fail(expressionContext(owner, enode) + ": expected " + std::to_string(count) +
                  " children, found " + std::to_string(enode->child.size()));
    }
    if (!allowNull) {
      for (size_t i = 0; i < enode->child.size(); i++) {
        if (!enode->child[i]) {
          return fail(expressionContext(owner, enode) + ": child " +
                      std::to_string(i) + " is null");
        }
      }
    }
    return true;
  }

  std::optional<LoweredValue> lowerChild(const Node* owner, const ENode* enode,
                                         size_t index,
                                         const std::optional<LoweredValue>& fallback = std::nullopt) {
    if (index >= enode->child.size() || !enode->child[index]) {
      fail(expressionContext(owner, enode) + ": missing child " + std::to_string(index));
      return std::nullopt;
    }
    return lowerExpression(owner, enode->child[index], fallback);
  }

  std::optional<IndexOperand> lowerIndexOperand(const Node* owner,
                                                const ENode* indexNode) {
    if (!indexNode) {
      fail(nodeContext(owner) + ": null array index");
      return std::nullopt;
    }
    IndexOperand index;
    if (indexNode->opType == OP_INDEX_INT) {
      if (!indexNode->child.empty() || indexNode->values.size() != 1) {
        fail(expressionContext(owner, indexNode) + ": malformed static array index");
        return std::nullopt;
      }
      index.staticValue = indexNode->values[0];
      return index;
    }
    if (indexNode->opType != OP_INDEX || indexNode->child.size() != 1 ||
        !indexNode->child[0]) {
      fail(expressionContext(owner, indexNode) +
           ": array index must be OP_INDEX_INT or OP_INDEX(expr)");
      return std::nullopt;
    }
    auto value = lowerExpression(owner, indexNode->child[0]);
    if (!value) return std::nullopt;
    if (value->isArray() || value->sign || value->width <= 0 || value->width > 64) {
      fail(expressionContext(owner, indexNode) +
           ": dynamic array index must be an unsigned scalar no wider than 64 bits");
      return std::nullopt;
    }
    index.dynamicValue = *value;
    return index;
  }

  std::optional<IndexPath> lowerIndexPath(const Node* owner, const Node* indexedNode,
                                          const ENode* reference,
                                          const LoweredValue& packed) {
    if (!reference || reference->nodePtr != indexedNode) {
      fail(nodeContext(owner) + ": indexed reference does not match target node");
      return std::nullopt;
    }
    if (indexedNode->dimension.empty()) {
      if (!reference->child.empty()) {
        fail(expressionContext(owner, reference) + ": scalar node has array indices");
        return std::nullopt;
      }
      IndexPath path;
      path.selectionShape = packed;
      path.staticBitOffset = 0;
      return path;
    }
    if (reference->child.size() > indexedNode->dimension.size()) {
      fail(expressionContext(owner, reference) + ": too many array indices");
      return std::nullopt;
    }

    std::vector<IndexOperand> indices;
    indices.reserve(reference->child.size());
    bool dynamic = false;
    int64_t staticFlat = 0;
    for (size_t i = 0; i < reference->child.size(); i++) {
      auto index = lowerIndexOperand(owner, reference->child[i]);
      if (!index) return std::nullopt;
      if (index->staticValue) {
        if (*index->staticValue < 0 ||
            *index->staticValue >= indexedNode->dimension[i]) {
          fail(expressionContext(owner, reference->child[i]) + ": static array index " +
               std::to_string(*index->staticValue) + " is outside [0, " +
               std::to_string(indexedNode->dimension[i]) + ")");
          return std::nullopt;
        }
        staticFlat = staticFlat * indexedNode->dimension[i] + *index->staticValue;
      } else {
        dynamic = true;
      }
      indices.push_back(*index);
    }

    IndexPath path;
    path.selectionShape = selectionShape(packed, reference->child.size());
    const int64_t remainingEntries = entryCount(path.selectionShape.dimensions);
    const int64_t bitScale = remainingEntries * packed.elementWidth;
    if (!dynamic) {
      path.staticBitOffset = staticFlat * bitScale;
      return path;
    }

    const int offsetWidth = bitsRequired(static_cast<uint64_t>(packed.width - 1));
    LoweredValue accumulator = emitUnsignedDecimalConstant(offsetWidth, 0);
    LoweredValue inRange = emitUnsignedDecimalConstant(1, 1);
    if (accumulator.symbol.empty() || inRange.symbol.empty()) return std::nullopt;

    for (size_t i = 0; i < indices.size(); i++) {
      LoweredValue indexValue;
      if (indices[i].staticValue) {
        indexValue = emitUnsignedDecimalConstant(
            offsetWidth, static_cast<uint64_t>(*indices[i].staticValue));
      } else {
        LoweredValue offsetShape = scalarValue({}, offsetWidth, false);
        auto offsetIndex = coerceToShape(owner, reference->child[i],
                                         indices[i].dynamicValue, offsetShape);
        if (!offsetIndex) return std::nullopt;
        indexValue = *offsetIndex;

        const int compareWidth = std::max(
            indices[i].dynamicValue.width,
            bitsRequired(static_cast<uint64_t>(indexedNode->dimension[i])));
        LoweredValue compareShape = scalarValue({}, compareWidth, false);
        auto compareIndex = coerceToShape(owner, reference->child[i],
                                          indices[i].dynamicValue, compareShape);
        if (!compareIndex) return std::nullopt;
        LoweredValue dimension = emitUnsignedDecimalConstant(
            compareWidth, static_cast<uint64_t>(indexedNode->dimension[i]));
        LoweredValue componentRange = emitTypedOperation(
            "kLt", {*compareIndex, dimension}, scalarValue({}, 1, false));
        inRange = emitTypedOperation("kAnd", {inRange, componentRange},
                                     scalarValue({}, 1, false));
      }
      if (indexValue.symbol.empty() || inRange.symbol.empty()) return std::nullopt;

      LoweredValue dimension = emitUnsignedDecimalConstant(
          offsetWidth, static_cast<uint64_t>(indexedNode->dimension[i]));
      accumulator = emitTypedOperation("kMul", {accumulator, dimension},
                                       scalarValue({}, offsetWidth, false));
      accumulator = emitTypedOperation("kAdd", {accumulator, indexValue},
                                       scalarValue({}, offsetWidth, false));
      if (accumulator.symbol.empty()) return std::nullopt;
    }
    if (bitScale != 1) {
      LoweredValue scale = emitUnsignedDecimalConstant(
          offsetWidth, static_cast<uint64_t>(bitScale));
      accumulator = emitTypedOperation("kMul", {accumulator, scale},
                                       scalarValue({}, offsetWidth, false));
      if (accumulator.symbol.empty()) return std::nullopt;
    }
    path.dynamicBitOffset = accumulator;
    path.inRange = inRange;
    return path;
  }

  std::optional<LoweredValue> extractIndexedValue(const Node* owner,
                                                  const ENode* reference,
                                                  const LoweredValue& packed,
                                                  const IndexPath& path) {
    if (path.staticBitOffset && *path.staticBitOffset == 0 &&
        path.selectionShape.width == packed.width) {
      LoweredValue result = path.selectionShape;
      result.symbol = packed.symbol;
      return result;
    }

    LoweredValue selected;
    if (path.staticBitOffset) {
      selected = emitStaticSlice(owner, reference, packed, *path.staticBitOffset,
                                 path.selectionShape);
    } else {
      selected = emitDynamicSlice(packed, path.dynamicBitOffset,
                                  path.selectionShape);
    }
    if (selected.symbol.empty()) return std::nullopt;
    if (path.inRange.symbol.empty()) return selected;

    LoweredValue zeroElement = emitConstant(path.selectionShape.elementWidth, false,
        std::to_string(path.selectionShape.elementWidth) + "'h0");
    auto zero = coerceToShape(owner, reference, zeroElement, path.selectionShape);
    if (!zero) return std::nullopt;
    LoweredValue guarded = emitTypedOperation(
        "kMux", {path.inRange, selected, *zero}, path.selectionShape);
    if (guarded.symbol.empty()) return std::nullopt;
    return guarded;
  }

  std::optional<LoweredValue> insertIndexedValue(const Node* owner,
                                                 const ENode* reference,
                                                 const LoweredValue& packed,
                                                 const IndexPath& path,
                                                 const LoweredValue& update) {
    auto coerced = coerceToShape(owner, reference, update, path.selectionShape);
    if (!coerced) return std::nullopt;
    if (path.staticBitOffset && *path.staticBitOffset == 0 &&
        path.selectionShape.width == packed.width) {
      LoweredValue result = packed;
      result.symbol = coerced->symbol;
      return result;
    }

    if (path.staticBitOffset) {
      const int64_t lowWidth = *path.staticBitOffset;
      const int64_t highStart = lowWidth + path.selectionShape.width;
      const int64_t highWidth = packed.width - highStart;
      std::vector<LoweredValue> operands;
      if (highWidth > 0) {
        LoweredValue high = emitStaticSlice(
            owner, reference, packed, highStart,
            scalarValue({}, static_cast<int>(highWidth), false));
        if (high.symbol.empty()) return std::nullopt;
        operands.push_back(high);
      }
      operands.push_back(*coerced);
      if (lowWidth > 0) {
        LoweredValue low = emitStaticSlice(
            owner, reference, packed, 0,
            scalarValue({}, static_cast<int>(lowWidth), false));
        if (low.symbol.empty()) return std::nullopt;
        operands.push_back(low);
      }
      LoweredValue result = emitConcat(owner, reference, operands, packed);
      if (result.symbol.empty()) return std::nullopt;
      return result;
    }

    LoweredValue baseMask = emitRangeOnesConstant(
        packed.width, 0, path.selectionShape.width - 1);
    if (baseMask.symbol.empty()) return std::nullopt;
    LoweredValue mask = emitTypedOperation(
        "kShl", {baseMask, path.dynamicBitOffset},
        scalarValue({}, packed.width, false));

    LoweredValue updateBitsShape = scalarValue({}, coerced->width, false);
    auto updateBits = coerceToShape(owner, reference, *coerced, updateBitsShape);
    if (!updateBits) return std::nullopt;
    auto updateFull = coerceToShape(owner, reference, *updateBits,
                                    scalarValue({}, packed.width, false));
    if (!updateFull) return std::nullopt;
    LoweredValue maskedUpdate = emitTypedOperation(
        "kAnd", {*updateFull, baseMask}, scalarValue({}, packed.width, false));
    LoweredValue shiftedUpdate = emitTypedOperation(
        "kShl", {maskedUpdate, path.dynamicBitOffset},
        scalarValue({}, packed.width, false));
    LoweredValue inverseMask = emitTypedOperation(
        "kNot", {mask}, scalarValue({}, packed.width, false));
    LoweredValue cleared = emitTypedOperation(
        "kAnd", {packed, inverseMask}, scalarValue({}, packed.width, false));
    LoweredValue updated = emitTypedOperation(
        "kOr", {cleared, shiftedUpdate}, scalarValue({}, packed.width, false));
    if (mask.symbol.empty() || maskedUpdate.symbol.empty() ||
        shiftedUpdate.symbol.empty() || inverseMask.symbol.empty() ||
        cleared.symbol.empty() || updated.symbol.empty()) {
      return std::nullopt;
    }

    LoweredValue result = packed;
    if (!path.inRange.symbol.empty()) {
      result = emitTypedOperation("kMux", {path.inRange, updated, packed}, packed);
      if (result.symbol.empty()) return std::nullopt;
    } else {
      result.symbol = updated.symbol;
    }
    return result;
  }

  std::optional<LoweredValue> lowerBinary(const Node* owner, const ENode* enode,
                                          const std::string& kind) {
    if (!expectChildren(owner, enode, 2, false)) return std::nullopt;
    auto lhs = lowerChild(owner, enode, 0);
    auto rhs = lowerChild(owner, enode, 1);
    if (!lhs || !rhs) return std::nullopt;
    if (lhs->isArray() || rhs->isArray()) {
      fail(expressionContext(owner, enode) + ": scalar binary op has array operand");
      return std::nullopt;
    }
    if (kind == "kConcat" && enode->width != lhs->width + rhs->width) {
      fail(expressionContext(owner, enode) +
           ": concat result width does not equal operand width sum");
      return std::nullopt;
    }
    LoweredValue result = emitExpressionOperation(enode, kind, {*lhs, *rhs});
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerConcat(const Node* owner,
                                          const ENode* enode) {
    if (!expectChildren(owner, enode, 2, false)) return std::nullopt;
    const ENode* lhsExpression = enode->child[0];
    const ENode* rhsExpression = enode->child[1];
    if (lhsExpression->width == 0 || rhsExpression->width == 0) {
      if (lhsExpression->width == 0 && rhsExpression->width == 0) {
        fail(expressionContext(owner, enode) +
             ": concat has no positive-width operand");
        return std::nullopt;
      }
      const ENode* survivor = lhsExpression->width == 0
          ? rhsExpression : lhsExpression;
      auto value = lowerExpression(owner, survivor, std::nullopt);
      if (!value || value->isArray()) {
        if (value) {
          fail(expressionContext(owner, enode) +
               ": zero-width concat elimination reached an array operand");
        }
        return std::nullopt;
      }
      return coerceToShape(owner, enode, *value,
                           scalarValue({}, enode->width, enode->sign));
    }
    return lowerBinary(owner, enode, "kConcat");
  }

  std::optional<LoweredValue> lowerUnary(const Node* owner, const ENode* enode,
                                         const std::string& kind) {
    if (!expectChildren(owner, enode, 1, false)) return std::nullopt;
    auto child = lowerChild(owner, enode, 0);
    if (!child) return std::nullopt;
    if (child->isArray()) {
      fail(expressionContext(owner, enode) + ": scalar unary op has array operand");
      return std::nullopt;
    }
    LoweredValue result = emitExpressionOperation(enode, kind, {*child});
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerAssignCast(const Node* owner,
                                              const ENode* enode,
                                              bool allowArrayFlatten) {
    if (!expectChildren(owner, enode, 1, false)) return std::nullopt;
    auto child = lowerChild(owner, enode, 0);
    if (!child) return std::nullopt;
    if (child->isArray() && (!allowArrayFlatten || child->width != enode->width)) {
      fail(expressionContext(owner, enode) +
           ": cast cannot preserve packed array width");
      return std::nullopt;
    }
    auto result = coerceToShape(owner, enode, *child,
                                scalarValue({}, enode->width, enode->sign));
    if (!result) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerShift(const Node* owner, const ENode* enode,
                                         bool left, bool dynamic) {
    const size_t childCount = dynamic ? 2 : 1;
    if (!expectChildren(owner, enode, childCount, false)) return std::nullopt;
    auto value = lowerChild(owner, enode, 0);
    if (!value || value->isArray()) {
      if (value) fail(expressionContext(owner, enode) + ": shift value is array-valued");
      return std::nullopt;
    }

    LoweredValue amount;
    if (dynamic) {
      auto loweredAmount = lowerChild(owner, enode, 1);
      if (!loweredAmount || loweredAmount->isArray() || loweredAmount->sign) {
        if (loweredAmount) {
          fail(expressionContext(owner, enode) +
               ": dynamic shift amount must be an unsigned scalar");
        }
        return std::nullopt;
      }
      amount = *loweredAmount;
    } else {
      if (enode->values.size() != 1 || enode->values[0] < 0) {
        fail(expressionContext(owner, enode) + ": invalid static shift immediate");
        return std::nullopt;
      }
      amount = emitUnsignedDecimalConstant(
          32, static_cast<uint64_t>(enode->values[0]));
      if (amount.symbol.empty()) return std::nullopt;
    }

    const std::string kind = left ? "kShl" : (value->sign ? "kAShr" : "kLShr");
    if (left) {
      auto widened = coerceToShape(owner, enode, *value,
                                   scalarValue({}, enode->width, enode->sign));
      if (!widened) return std::nullopt;
      LoweredValue result = emitTypedOperation(
          kind, {*widened, amount}, scalarValue({}, enode->width, enode->sign));
      if (result.symbol.empty()) return std::nullopt;
      return result;
    }

    LoweredValue shifted = emitTypedOperation(
        kind, {*value, amount}, scalarValue({}, value->width, value->sign));
    if (shifted.symbol.empty()) return std::nullopt;
    if (shifted.width == enode->width && shifted.sign == enode->sign) return shifted;
    if (enode->width <= 0 || enode->width > shifted.width) {
      fail(expressionContext(owner, enode) + ": invalid narrowed shift width");
      return std::nullopt;
    }
    LoweredValue result = emitStaticSlice(
        owner, enode, shifted, 0,
        scalarValue({}, enode->width, enode->sign));
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerHeadOrTail(const Node* owner,
                                              const ENode* enode,
                                              bool head) {
    if (!expectChildren(owner, enode, 1, false) || enode->values.size() != 1) {
      fail(expressionContext(owner, enode) + ": malformed head/tail operation");
      return std::nullopt;
    }
    auto child = lowerChild(owner, enode, 0);
    if (!child || child->isArray()) {
      if (child) fail(expressionContext(owner, enode) + ": head/tail operand is array-valued");
      return std::nullopt;
    }
    if (enode->width <= 0 || enode->width > child->width) {
      fail(expressionContext(owner, enode) + ": invalid head/tail result width");
      return std::nullopt;
    }
    const int64_t start = head ? enode->values[0] : 0;
    const int expectedValue = head ? child->width - enode->width : enode->width;
    if (enode->values[0] != expectedValue) {
      fail(expressionContext(owner, enode) +
           ": optimized head/tail immediate is inconsistent with widths");
      return std::nullopt;
    }
    LoweredValue result = emitStaticSlice(
        owner, enode, *child, start,
        scalarValue({}, enode->width, enode->sign));
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerBitsNoShift(const Node* owner,
                                              const ENode* enode) {
    if (!expectChildren(owner, enode, 1, false) || enode->values.size() != 2) {
      fail(expressionContext(owner, enode) + ": malformed no-shift bit range");
      return std::nullopt;
    }
    auto child = lowerChild(owner, enode, 0);
    if (!child || child->isArray()) {
      if (child) fail(expressionContext(owner, enode) + ": bit-range operand is array-valued");
      return std::nullopt;
    }
    const int high = enode->values[0];
    const int low = enode->values[1];
    if (low < 0 || high < low || high >= child->width || enode->width != high + 1) {
      fail(expressionContext(owner, enode) + ": invalid no-shift bit range [" +
           std::to_string(high) + ":" + std::to_string(low) +
           "] for source width " + std::to_string(child->width));
      return std::nullopt;
    }
    LoweredValue lowBits;
    if (child->width == enode->width) {
      auto coerced = coerceToShape(owner, enode, *child,
                                   scalarValue({}, enode->width, false));
      if (!coerced) return std::nullopt;
      lowBits = *coerced;
    } else {
      lowBits = emitStaticSlice(owner, enode, *child, 0,
                                scalarValue({}, enode->width, false));
    }
    LoweredValue mask = emitRangeOnesConstant(enode->width, low, high);
    LoweredValue result = emitTypedOperation(
        "kAnd", {lowBits, mask}, scalarValue({}, enode->width, enode->sign));
    if (lowBits.symbol.empty() || mask.symbol.empty() || result.symbol.empty()) {
      return std::nullopt;
    }
    return result;
  }

  std::optional<LoweredValue> lowerGroup(const Node* owner,
                                         const ENode* enode) {
    if (enode->child.empty()) {
      fail(expressionContext(owner, enode) + ": empty array group");
      return std::nullopt;
    }
    std::vector<LoweredValue> children;
    children.reserve(enode->child.size());
    for (size_t i = 0; i < enode->child.size(); i++) {
      auto child = lowerChild(owner, enode, i);
      if (!child) return std::nullopt;
      children.push_back(*child);
    }
    const LoweredValue& element = children.front();
    for (const LoweredValue& child : children) {
      if (child.width != element.width || child.sign != element.sign ||
          child.dimensions != element.dimensions ||
          child.elementWidth != element.elementWidth ||
          child.elementSign != element.elementSign) {
        fail(expressionContext(owner, enode) +
             ": array group children have incompatible shapes");
        return std::nullopt;
      }
    }

    LoweredValue result;
    result.elementWidth = element.elementWidth;
    result.elementSign = element.elementSign;
    result.dimensions.push_back(static_cast<int>(children.size()));
    result.dimensions.insert(result.dimensions.end(), element.dimensions.begin(),
                             element.dimensions.end());
    result.width = element.width * static_cast<int>(children.size());
    result.sign = false;
    std::reverse(children.begin(), children.end());
    LoweredValue packed = emitConcat(owner, enode, children, result);
    if (packed.symbol.empty()) return std::nullopt;
    return packed;
  }

  std::optional<LoweredValue> lowerWhen(const Node* owner, const ENode* enode,
                                        const std::optional<LoweredValue>& fallback,
                                        bool requireBothBranches) {
    if (!expectChildren(owner, enode, 3, !requireBothBranches)) return std::nullopt;
    if (!enode->child[0]) {
      fail(expressionContext(owner, enode) + ": condition child is null");
      return std::nullopt;
    }
    auto cond = lowerExpression(owner, enode->child[0], std::nullopt);
    if (!cond) return std::nullopt;
    if (cond->width != 1) {
      fail(expressionContext(owner, enode) + ": condition width is not 1");
      return std::nullopt;
    }

    auto lowerBranch = [&](size_t index) -> std::optional<LoweredValue> {
      if (enode->child[index]) return lowerExpression(owner, enode->child[index], fallback);
      if (fallback) return fallback;
      fail(expressionContext(owner, enode) + ": null branch " + std::to_string(index) +
           " has no prior lvalue value to preserve");
      return std::nullopt;
    };
    auto trueValue = lowerBranch(1);
    auto falseValue = lowerBranch(2);
    if (!trueValue || !falseValue) return std::nullopt;

    LoweredValue resultShape;
    if (fallback && fallback->isArray()) resultShape = *fallback;
    else if (trueValue->isArray()) resultShape = *trueValue;
    else if (falseValue->isArray()) resultShape = *falseValue;
    else resultShape = scalarValue({}, enode->width, enode->sign);
    resultShape.symbol.clear();

    auto coercedTrue = coerceToShape(owner, enode, *trueValue, resultShape);
    auto coercedFalse = coerceToShape(owner, enode, *falseValue, resultShape);
    if (!coercedTrue || !coercedFalse) return std::nullopt;
    if (coercedTrue->symbol == coercedFalse->symbol) {
      resultShape.symbol = coercedTrue->symbol;
      return resultShape;
    }
    LoweredValue result = emitTypedOperation(
        "kMux", {*cond, *coercedTrue, *coercedFalse}, resultShape);
    if (result.symbol.empty()) return std::nullopt;
    return result;
  }

  std::optional<LoweredValue> lowerExpression(
      const Node* owner, const ENode* enode,
      const std::optional<LoweredValue>& fallback = std::nullopt) {
    if (!enode) {
      fail(nodeContext(owner) + ": null expression root");
      return std::nullopt;
    }
    if (!activeExpressions_.insert(enode).second) {
      fail(expressionContext(owner, enode) + ": cyclic expression tree");
      return std::nullopt;
    }
    ActiveExpressionGuard guard{activeExpressions_, enode};
    EnodeContextGuard enodeGuard{enodeContextStack_, enodeContextKey(enode)};
    enodeVisitCounts_[enodeContextStack_.back()]++;

    if (enode->width == 0 && enode->opType == OP_INT &&
        !enode->nodePtr && enode->child.empty()) {
      if (!fallback || fallback->isArray() || fallback->width <= 0) {
        fail(expressionContext(owner, enode) +
             ": zero-width integer literal has no positive-width scalar context");
        return std::nullopt;
      }
      LoweredValue zero = emitConstant(
          fallback->width, fallback->sign,
          std::to_string(fallback->width) + "'h0");
      if (zero.symbol.empty()) return std::nullopt;
      return zero;
    }

    if (enode->nodePtr) {
      if (enode->nodePtr->type == NODE_MEMORY ||
          enode->nodePtr->type == NODE_WRITER) {
        fail(expressionContext(owner, enode) + ": references non-value " +
             nodeContext(enode->nodePtr));
        return std::nullopt;
      }
      auto found = nodeValues_.find(enode->nodePtr);
      if (found == nodeValues_.end()) {
        fail(expressionContext(owner, enode) + ": references unavailable " +
             nodeContext(enode->nodePtr));
        return std::nullopt;
      }
      auto path = lowerIndexPath(owner, enode->nodePtr, enode, found->second);
      if (!path) return std::nullopt;
      auto extracted = extractIndexedValue(owner, enode, found->second, *path);
      if (!extracted) return std::nullopt;
      if (extracted->isArray()) {
        if (enode->width != extracted->elementWidth ||
            enode->sign != extracted->elementSign) {
          fail(expressionContext(owner, enode) +
               ": implicit array element width/sign conversion is not supported");
          return std::nullopt;
        }
        return extracted;
      }
      return coerceToShape(owner, enode, *extracted,
                           scalarValue({}, enode->width, enode->sign));
    }
    if (enode->width <= 0 && enode->opType != OP_INVALID &&
        enode->opType != OP_INDEX_INT && enode->opType != OP_INDEX) {
      fail(expressionContext(owner, enode) + ": expression width must be positive");
      return std::nullopt;
    }

    switch (enode->opType) {
      case OP_INT: {
        if (!expectChildren(owner, enode, 0, false)) return std::nullopt;
        auto literal = canonicalFirrtlLiteral(enode, owner);
        if (!literal) return std::nullopt;
        LoweredValue result = emitConstant(enode->width, enode->sign, *literal);
        if (result.symbol.empty()) return std::nullopt;
        return result;
      }
      case OP_INVALID:
        if (fallback) return fallback;
        fail(expressionContext(owner, enode) +
             ": invalid value has no prior lvalue value to preserve");
        return std::nullopt;
      case OP_INDEX_INT: {
        if (!enode->child.empty() || enode->values.size() != 1 ||
            enode->values[0] < 0) {
          fail(expressionContext(owner, enode) + ": malformed standalone static index");
          return std::nullopt;
        }
        LoweredValue result = emitUnsignedDecimalConstant(
            bitsRequired(static_cast<uint64_t>(enode->values[0])),
            static_cast<uint64_t>(enode->values[0]));
        if (result.symbol.empty()) return std::nullopt;
        return result;
      }
      case OP_INDEX: {
        if (!expectChildren(owner, enode, 1, false)) return std::nullopt;
        auto result = lowerChild(owner, enode, 0);
        if (!result || result->isArray() || result->sign || result->width > 64) {
          if (result) {
            fail(expressionContext(owner, enode) +
                 ": standalone dynamic index must be an unsigned scalar no wider than 64 bits");
          }
          return std::nullopt;
        }
        return result;
      }
      case OP_MUX:
        return lowerWhen(owner, enode, std::nullopt, true);
      case OP_WHEN:
        return lowerWhen(owner, enode, fallback, false);
      case OP_ADD: return lowerBinary(owner, enode, "kAdd");
      case OP_SUB: return lowerBinary(owner, enode, "kSub");
      case OP_MUL: return lowerBinary(owner, enode, "kMul");
      case OP_DIV: return lowerBinary(owner, enode, "kDiv");
      case OP_REM: return lowerBinary(owner, enode, "kMod");
      case OP_LT: return lowerBinary(owner, enode, "kLt");
      case OP_LEQ: return lowerBinary(owner, enode, "kLe");
      case OP_GT: return lowerBinary(owner, enode, "kGt");
      case OP_GEQ: return lowerBinary(owner, enode, "kGe");
      case OP_EQ: return lowerBinary(owner, enode, "kEq");
      case OP_NEQ: return lowerBinary(owner, enode, "kNe");
      case OP_DSHL: return lowerShift(owner, enode, true, true);
      case OP_DSHR: return lowerShift(owner, enode, false, true);
      case OP_AND: return lowerBinary(owner, enode, "kAnd");
      case OP_OR: return lowerBinary(owner, enode, "kOr");
      case OP_XOR: return lowerBinary(owner, enode, "kXor");
      case OP_CAT: return lowerConcat(owner, enode);
      case OP_NOT: return lowerUnary(owner, enode, "kNot");
      case OP_ANDR: return lowerUnary(owner, enode, "kReduceAnd");
      case OP_ORR: return lowerUnary(owner, enode, "kReduceOr");
      case OP_XORR: return lowerUnary(owner, enode, "kReduceXor");
      case OP_ASUINT:
      case OP_ASSINT:
        return lowerAssignCast(owner, enode, true);
      case OP_ASCLOCK:
      case OP_ASASYNCRESET:
      case OP_CVT:
      case OP_PAD:
      case OP_SEXT:
        return lowerAssignCast(owner, enode, false);
      case OP_NEG: {
        if (!expectChildren(owner, enode, 1, false)) return std::nullopt;
        auto child = lowerChild(owner, enode, 0);
        if (!child) return std::nullopt;
        LoweredValue zero = emitConstant(enode->width, enode->sign,
                                        std::to_string(enode->width) + "'h0");
        if (zero.symbol.empty()) return std::nullopt;
        LoweredValue result = emitExpressionOperation(enode, "kSub", {zero, *child});
        if (result.symbol.empty()) return std::nullopt;
        return result;
      }
      case OP_SHL: return lowerShift(owner, enode, true, false);
      case OP_SHR: return lowerShift(owner, enode, false, false);
      case OP_BITS: {
        if (!expectChildren(owner, enode, 1, false) || enode->values.size() != 2 ||
            enode->values[0] < enode->values[1] || enode->values[1] < 0) {
          fail(expressionContext(owner, enode) + ": invalid static slice range");
          return std::nullopt;
        }
        auto child = lowerChild(owner, enode, 0);
        if (!child) return std::nullopt;
        if (child->isArray() || enode->values[0] >= child->width ||
            enode->width != enode->values[0] - enode->values[1] + 1) {
          fail(expressionContext(owner, enode) + ": static bit range exceeds source width");
          return std::nullopt;
        }
        LoweredValue result = emitStaticSlice(
            owner, enode, *child, enode->values[1],
            scalarValue({}, enode->width, enode->sign));
        if (result.symbol.empty()) return std::nullopt;
        return result;
      }
      case OP_EMPTY:
        fail(expressionContext(owner, enode) + ": empty non-reference expression");
        return std::nullopt;
      case OP_HEAD: return lowerHeadOrTail(owner, enode, true);
      case OP_TAIL: return lowerHeadOrTail(owner, enode, false);
      case OP_BITS_NOSHIFT: return lowerBitsNoShift(owner, enode);
      case OP_GROUP: return lowerGroup(owner, enode);
      case OP_READ_MEM:
      case OP_WRITE_MEM:
      case OP_INFER_MEM:
        fail(expressionContext(owner, enode) +
             ": memory operation requires the executable-memory extension");
        return std::nullopt;
      case OP_PRINTF:
      case OP_ASSERT:
      case OP_EXIT:
        fail(expressionContext(owner, enode) +
             ": side-effect operation requires the executable-effects extension");
        return std::nullopt;
      case OP_EXT_FUNC:
        fail(expressionContext(owner, enode) +
             ": external call requires the executable-blackbox extension");
        return std::nullopt;
      case OP_RESET:
        fail(expressionContext(owner, enode) +
             ": OP_RESET is only valid in a register resetTree");
        return std::nullopt;
      case OP_STMT_SEQ:
      case OP_STMT_WHEN:
      case OP_STMT_NODE:
        fail(expressionContext(owner, enode) +
             ": post-precoarsen statement op is invalid at this export boundary");
        return std::nullopt;
    }
    fail(expressionContext(owner, enode) + ": unknown operation");
    return std::nullopt;
  }

  bool validateLvalue(const Node* owner, const ExpTree* tree, size_t treeIndex) {
    if (!tree) {
      return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) + "] is null");
    }
    const ENode* lvalue = tree->getlval();
    if (!lvalue || lvalue->nodePtr != owner) {
      return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) +
                  "] has mismatched lvalue");
    }
    if (owner->dimension.empty() && !lvalue->child.empty()) {
      return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) +
                  "] indexes a scalar lvalue");
    }
    if (lvalue->child.size() > owner->dimension.size()) {
      return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) +
                  "] has too many array indices");
    }
    for (const ENode* index : lvalue->child) {
      if (!index || (index->opType != OP_INDEX_INT && index->opType != OP_INDEX)) {
        return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) +
                    "] has unsupported array index expression");
      }
    }
    if (!tree->getRoot()) {
      return fail(nodeContext(owner) + ": assignTree[" + std::to_string(treeIndex) +
                  "] has null root");
    }
    return true;
  }

  bool lowerAssignedNode(Node* node) {
    if (nodeEmissionCapture_) {
      return fail(nodeContext(node) + ": nested executable GRH node emission capture");
    }
    NodeContextGuard nodeGuard{nodeContextStack_, node};
    if (node->type == NODE_REG_DST) {
      auto srcIt = registerDestinations_.find(node);
      if (srcIt != registerDestinations_.end() &&
          usePerLeafRegisterWrites(node)) {
        /* vector register with when-skeleton updates: the functional merge
           is replaced by per-leaf masked write ports in emitRegisterWrites,
           so no merged nextValue is materialized here */
        assignedNodes_.insert(node);
        return true;
      }
    }
    nodeEmissionCapture_.emplace();
    const LoweredValue target = nodeValues_.at(node);
    std::optional<LoweredValue> current;
    if (node->type == NODE_REG_DST) {
      auto srcIt = registerDestinations_.find(node);
      if (srcIt == registerDestinations_.end()) {
        return fail(nodeContext(node) + ": missing register source for hold value");
      }
      current = nodeValues_.at(srcIt->second);
    } else {
      // FIRRTL invalid / missing conditional branches have no four-state value
      // in GSim's two-state runtime.  Use the same deterministic zero base for
      // scalar leaves (including split array elements) that arrays already use;
      // subsequent assignment trees override it in source order.
      LoweredValue zero = emitConstant(target.width, false,
                                       std::to_string(target.width) + "'h0");
      if (zero.symbol.empty()) return false;
      zero.elementWidth = target.elementWidth;
      zero.elementSign = target.elementSign;
      zero.dimensions = target.dimensions;
      current = zero;
    }
    for (size_t i = 0; i < node->assignTree.size(); i++) {
      ExpTree* tree = node->assignTree[i];
      EnodeContextGuard treeGuard{enodeContextStack_,
                                  enodeContextKey(tree ? tree->getRoot() : nullptr)};
      if (!validateLvalue(node, tree, i)) return false;
      auto path = lowerIndexPath(node, node, tree->getlval(), target);
      if (!path) return false;
      std::optional<LoweredValue> fallback;
      if (current) {
        fallback = extractIndexedValue(node, tree->getlval(), *current, *path);
        if (!fallback) return false;
      }
      auto lowered = lowerExpression(node, tree->getRoot(), fallback);
      if (!lowered) return false;
      const LoweredValue& base = current ? *current : target;
      auto inserted = insertIndexedValue(node, tree->getlval(), base, *path, *lowered);
      if (!inserted) return false;
      current = *inserted;
    }
    if (!current) return fail(nodeContext(node) + ": live assigned node has no assignment tree");

    const ENode* coercionContext = node->assignTree.empty()
        ? nullptr : node->assignTree.back()->getRoot();
    EnodeContextGuard anchorGuard{enodeContextStack_,
                                  enodeContextKey(coercionContext)};
    auto coerced = coerceToShape(node, coercionContext, *current, target);
    if (!coerced) return false;
    current = *coerced;
    if (current->symbol == target.symbol) {
      return fail(nodeContext(node) + ": assignment would leave its GRH value undefined/self-referential");
    }
    std::vector<JsonAttr> attributes = {
        intAttr("gsim.node_id", node->id),
        stringAttr("gsim.node_name", node->name)};
    if (node->assignTree.empty()) {
      attributes.push_back(boolAttr("gsim.empty_assignment_zero", true));
    }
    if (constantOutputNodes_.count(node)) {
      attributes.push_back(boolAttr("gsim.constant_output", true));
    }
    const FinalAssignDisposition disposition =
        finishNodeEmissionCapture(*current, target, attributes);
    if (disposition == FinalAssignDisposition::Error) return false;
    if (disposition == FinalAssignDisposition::Elided) {
      assignedNodes_.insert(node);
      return true;
    }
    if (!emitOperation("gsim.assign." + std::to_string(node->id), "kAssign",
                       {current->symbol}, {target.symbol},
                       std::move(attributes))) {
      return false;
    }
    assignedNodes_.insert(node);
    return true;
  }

  std::optional<LoweredValue> lowerEffectCondition(
      Node* node, const executable_grh::EffectPlan& plan) {
    std::optional<LoweredValue> condition;
    for (const executable_grh::EffectGuardTerm& term : plan.guards) {
      auto lowered = lowerExpression(node, term.expression);
      if (!lowered) return std::nullopt;
      if (lowered->isArray() || lowered->width != 1) {
        fail(nodeContext(node) + ": executable effect guard did not lower to one-bit logic");
        return std::nullopt;
      }
      if (!term.expectedValue) {
        *lowered = emitTypedOperation("kNot", {*lowered},
                                     scalarValue({}, 1, false));
        if (lowered->symbol.empty()) return std::nullopt;
      }
      if (!condition) {
        condition = *lowered;
      } else {
        *condition = emitTypedOperation("kAnd", {*condition, *lowered},
                                        scalarValue({}, 1, false));
        if (condition->symbol.empty()) return std::nullopt;
      }
    }
    if (!condition) {
      fail(nodeContext(node) + ": executable effect has no call guard");
      return std::nullopt;
    }
    return condition;
  }

  bool lowerAndEmitEffect(Node* node) {
    auto planIt = effectPlans_.find(node);
    if (planIt == effectPlans_.end()) {
      return fail(nodeContext(node) + ": executable effect reached lowering without a plan");
    }
    const executable_grh::EffectPlan& plan = planIt->second;
    NodeContextGuard nodeGuard{nodeContextStack_, node};
    EnodeContextGuard effectGuard{enodeContextStack_, effectContextKey(plan.kind)};
    auto condition = lowerEffectCondition(node, plan);
    if (!condition) return false;

    std::vector<std::string> inputs = {condition->symbol};
    switch (plan.kind) {
      case executable_grh::EffectKind::Printf: {
        if (plan.systemTaskName != "fwrite" || !plan.prependStderrHandle ||
            plan.hasExitCode) {
          return fail(nodeContext(node) + ": invalid printf executable effect plan");
        }
        LoweredValue stderrHandle = emitUnsignedDecimalConstant(32, 2);
        const std::string format = emitStringConstant(plan.formatText);
        if (stderrHandle.symbol.empty() || format.empty()) return false;
        inputs.push_back(stderrHandle.symbol);
        inputs.push_back(format);
        for (const ENode* argument : plan.arguments) {
          auto lowered = lowerExpression(node, argument);
          if (!lowered) return false;
          if (lowered->isArray() || lowered->width <= 0) {
            return fail(nodeContext(node) +
                        ": printf argument did not lower to positive-width scalar logic");
          }
          inputs.push_back(lowered->symbol);
        }
        break;
      }
      case executable_grh::EffectKind::Assert: {
        if (plan.systemTaskName != "fatal" || plan.prependStderrHandle ||
            !plan.hasExitCode || plan.exitCode != 1 || !plan.arguments.empty()) {
          return fail(nodeContext(node) + ": invalid assert executable effect plan");
        }
        LoweredValue exitCode = emitUnsignedDecimalConstant(32, 1);
        const std::string format = emitStringConstant(plan.formatText);
        if (exitCode.symbol.empty() || format.empty()) return false;
        inputs.push_back(exitCode.symbol);
        inputs.push_back(format);
        break;
      }
      case executable_grh::EffectKind::Exit: {
        if (plan.systemTaskName != "finish" || plan.prependStderrHandle ||
            !plan.hasExitCode || !plan.arguments.empty()) {
          return fail(nodeContext(node) + ": invalid exit executable effect plan");
        }
        LoweredValue exitCode = emitUnsignedDecimalConstant(
            32, static_cast<uint32_t>(plan.exitCode));
        if (exitCode.symbol.empty()) return false;
        inputs.push_back(exitCode.symbol);
        break;
      }
    }

    auto clock = nodeValues_.find(plan.baseClock);
    if (clock == nodeValues_.end() || clock->second.isArray() ||
        clock->second.width != 1) {
      return fail(nodeContext(node) +
                  ": executable effect base clock is unavailable or not one bit");
    }
    inputs.push_back(clock->second.symbol);
    std::vector<JsonAttr> attributes = {
        stringAttr("name", plan.systemTaskName),
        stringAttr("procKind", "always_ff"),
        boolAttr("hasTiming", false),
        stringListAttr("eventEdge", {"posedge"}),
        intAttr("gsim.node_id", node->id),
        stringAttr("gsim.node_name", node->name),
        stringAttr("gsim.effect_kind", executable_grh::effectKindName(plan.kind))};
    if (plan.literalizedUnboundFormatConversions) {
      attributes.push_back(boolAttr("gsim.unbound_format_conversions_literalized", true));
    }
    return emitOperation("gsim.effect." + std::to_string(node->id), "kSystemTask",
                         inputs, {}, std::move(attributes));
  }

  std::string dpiImportSignature(const executable_grh::DpiCallPlan& call) const {
    std::ostringstream stream;
    stream << call.importSymbol << '|';
    for (const auto& argument : call.arguments) {
      stream << static_cast<int>(argument.direction) << ':' << argument.name << ':'
             << argument.type << ':' << argument.width << ':' << argument.isSigned << ';';
    }
    stream << "ret:" << call.hasReturn << ':' << call.returnType << ':'
           << call.returnWidth << ':' << call.returnSigned;
    return stream.str();
  }

  bool emitDpiImport(const executable_grh::DpiCallPlan& call) {
    const std::string signature = dpiImportSignature(call);
    auto found = emittedDpiImportSignatures_.find(call.importSymbol);
    if (found != emittedDpiImportSignatures_.end()) {
      if (found->second != signature) {
        return fail("conflicting executable DPI signatures for import '" +
                    call.importSymbol + "'");
      }
      return true;
    }

    std::vector<std::string> directions;
    std::vector<int64_t> widths;
    std::vector<std::string> names;
    std::vector<bool> signs;
    std::vector<std::string> types;
    for (const auto& argument : call.arguments) {
      directions.push_back(argument.direction == executable_grh::DpiArgDirection::Input
                               ? "input" : "output");
      widths.push_back(argument.width);
      names.push_back(argument.name);
      signs.push_back(argument.isSigned);
      types.push_back(argument.type);
    }
    std::vector<JsonAttr> attrs = {
        stringListAttr("argsDirection", std::move(directions)),
        intListAttr("argsWidth", std::move(widths)),
        stringListAttr("argsName", std::move(names)),
        boolListAttr("argsSigned", std::move(signs)),
        stringListAttr("argsType", std::move(types)),
        boolAttr("hasReturn", call.hasReturn),
    };
    if (call.hasReturn) {
      attrs.push_back(intAttr("returnWidth", call.returnWidth));
      attrs.push_back(boolAttr("returnSigned", call.returnSigned));
      attrs.push_back(stringAttr("returnType", call.returnType));
    }
    if (!emitOperation(call.importSymbol, "kDpicImport", {}, {}, attrs)) return false;
    emittedDpiImportSignatures_.emplace(call.importSymbol, signature);
    return true;
  }

  std::optional<LoweredValue> lowerExternalMemberElement(
      const ExternalInstance& instance,
      const executable_grh::DpiArgumentPlan& argument) {
    if (argument.source.index >= instance.root->member.size()) {
      fail(nodeContext(instance.root) + ": external member argument index is out of range");
      return std::nullopt;
    }
    Node* member = instance.root->member[argument.source.index];
    auto found = nodeValues_.find(member);
    if (found == nodeValues_.end()) {
      fail(nodeContext(member) + ": external member value is unavailable");
      return std::nullopt;
    }
    LoweredValue value = found->second;
    if (value.isArray()) {
      const int64_t elements = entryCount(value.dimensions);
      if (argument.source.element >= static_cast<size_t>(elements)) {
        fail(nodeContext(member) + ": external array element is out of range");
        return std::nullopt;
      }
      const int64_t bitOffset =
          static_cast<int64_t>(argument.source.element) * value.elementWidth;
      value = emitStaticSlice(
          instance.root, instance.root->assignTree.front()->getRoot(), value,
          bitOffset, scalarValue({}, value.elementWidth, value.elementSign));
      if (value.symbol.empty()) return std::nullopt;
    } else if (argument.source.element != 0) {
      fail(nodeContext(member) + ": scalar external member uses a nonzero element index");
      return std::nullopt;
    }
    auto coerced = coerceToShape(
        instance.root, instance.root->assignTree.front()->getRoot(), value,
        scalarValue({}, static_cast<int>(argument.width), argument.isSigned));
    if (!coerced) return std::nullopt;
    return *coerced;
  }

  std::optional<LoweredValue> lowerExternalInputArgument(
      const ExternalInstance& instance,
      const executable_grh::DpiArgumentPlan& argument) {
    switch (argument.source.kind) {
      case executable_grh::DpiValueSourceKind::Member:
        return lowerExternalMemberElement(instance, argument);
      case executable_grh::DpiValueSourceKind::Literal: {
        LoweredValue value = emitConstant(
            static_cast<int>(argument.width), argument.isSigned,
            argument.source.literal);
        if (value.symbol.empty()) return std::nullopt;
        return value;
      }
      case executable_grh::DpiValueSourceKind::Parameter:
        fail(nodeContext(instance.root) +
             ": parameter-sourced executable DPI arguments are not yet supported");
        return std::nullopt;
    }
    fail(nodeContext(instance.root) + ": unknown executable DPI argument source");
    return std::nullopt;
  }

  std::optional<LoweredValue> lowerExternalCallCondition(
      const ExternalInstance& instance,
      const executable_grh::DpiCallPlan& call) {
    LoweredValue condition = emitConstant(1, false, "1'h1");
    if (condition.symbol.empty()) return std::nullopt;
    for (size_t memberIndex : call.conditionMembers) {
      Node* member = instance.root->member[memberIndex];
      auto found = nodeValues_.find(member);
      if (found == nodeValues_.end() || found->second.isArray() ||
          found->second.width != 1) {
        fail(nodeContext(member) +
             ": external call condition value is unavailable or not scalar one-bit");
        return std::nullopt;
      }
      condition = emitTypedOperation(
          "kAnd", {condition, found->second}, scalarValue({}, 1, false));
      if (condition.symbol.empty()) return std::nullopt;
    }
    return condition;
  }

  bool assignExternalOutput(Node* root, size_t memberIndex,
                            const LoweredValue& source,
                            const std::string& role) {
    if (memberIndex >= root->member.size()) {
      return fail(nodeContext(root) + ": external output assignment index is out of range");
    }
    Node* member = root->member[memberIndex];
    if (member->type != NODE_EXT_OUT) {
      return fail(nodeContext(member) + ": external plan attempted to assign an input member");
    }
    NodeContextGuard nodeGuard{nodeContextStack_, member};
    const LoweredValue& target = nodeValues_.at(member);
    auto coerced = coerceToShape(
        root, root->assignTree.front()->getRoot(), source, target);
    if (!coerced) return false;
    if (!emitOperation(
            "gsim.ext_assign." + std::to_string(root->id) + "." +
                std::to_string(memberIndex),
            "kAssign", {coerced->symbol}, {target.symbol},
            {intAttr("gsim.node_id", member->id),
             stringAttr("gsim.external_role", role)})) {
      return false;
    }
    assignedNodes_.insert(member);
    return true;
  }

  bool lowerAndEmitExternalInstance(ExternalInstance& instance) {
    Node* root = instance.root;
    NodeContextGuard nodeGuard{nodeContextStack_, root};
    EnodeContextGuard extGuard{enodeContextStack_, std::string("OP_EXT_FUNC")};
    for (size_t callIndex = 0; callIndex < instance.plan.calls.size(); ++callIndex) {
      const auto& call = instance.plan.calls[callIndex];
      if (!emitDpiImport(call)) return false;
      auto condition = lowerExternalCallCondition(instance, call);
      if (!condition) return false;

      std::vector<std::string> inputs = {condition->symbol};
      std::vector<std::string> inputNames;
      std::vector<std::string> outputNames;
      struct OutputResult {
        size_t memberIndex;
        LoweredValue value;
      };
      std::vector<OutputResult> outputResults;

      for (const auto& argument : call.arguments) {
        if (argument.direction == executable_grh::DpiArgDirection::Input) {
          auto value = lowerExternalInputArgument(instance, argument);
          if (!value) return false;
          inputs.push_back(value->symbol);
          inputNames.push_back(argument.name);
        } else {
          if (argument.source.kind != executable_grh::DpiValueSourceKind::Member) {
            return fail(nodeContext(root) +
                        ": executable DPI output must target an external member");
          }
          LoweredValue result = scalarValue(
              nextTemporaryValueSymbol(), static_cast<int>(argument.width),
              argument.isSigned);
          if (!emitValue(result)) return false;
          outputNames.push_back(argument.name);
          outputResults.push_back(OutputResult{argument.source.index, result});
        }
      }

      std::vector<std::string> outputs;
      std::optional<LoweredValue> returnResult;
      if (call.hasReturn) {
        LoweredValue result = scalarValue(
            nextTemporaryValueSymbol(), static_cast<int>(call.returnWidth),
            call.returnSigned);
        if (!emitValue(result)) return false;
        outputs.push_back(result.symbol);
        returnResult = result;
      }
      for (const OutputResult& output : outputResults) {
        outputs.push_back(output.value.symbol);
      }

      std::vector<std::string> eventEdges;
      if (call.eventEdge != executable_grh::DpiEventEdge::None) {
        auto clock = nodeValues_.find(root->clock);
        if (clock == nodeValues_.end() || clock->second.isArray() ||
            clock->second.width != 1) {
          return fail(nodeContext(root) +
                      ": external module clock is unavailable or not scalar one-bit");
        }
        inputs.push_back(clock->second.symbol);
        eventEdges.push_back(executable_grh::dpiEventEdgeName(call.eventEdge));
      }

      if (!emitOperation(
              "gsim.dpi_call." + std::to_string(root->id) + "." +
                  std::to_string(callIndex),
              "kDpicCall", inputs, outputs,
              {stringAttr("targetImportSymbol", call.importSymbol),
               stringListAttr("inArgName", inputNames),
               stringListAttr("outArgName", outputNames),
               stringListAttr("inoutArgName", {}),
               boolAttr("hasReturn", call.hasReturn),
               stringListAttr("eventEdge", eventEdges),
               boolAttr("hasSideEffects", true),
               stringAttr("gsim.external_instance_group",
                          "gsim.external." + std::to_string(root->id)),
               intAttr("gsim.external_call_ordinal",
                       static_cast<int64_t>(callIndex)),
               intAttr("gsim.external_node_id", root->id),
               stringAttr("gsim.external_defname", instance.abi.defname)})) {
        return false;
      }

      if (returnResult) {
        LoweredValue result = *returnResult;
        if (!call.inactiveReturnLiteral.empty()) {
          LoweredValue inactive = emitConstant(
              static_cast<int>(call.returnWidth), call.returnSigned,
              call.inactiveReturnLiteral);
          if (inactive.symbol.empty()) return false;
          result = emitTypedOperation(
              "kMux", {*condition, result, inactive}, result,
              {stringAttr("gsim.role", "external_inactive_return")});
          if (result.symbol.empty()) return false;
        }
        if (!assignExternalOutput(root, call.returnMember, result, "dpi_return")) {
          return false;
        }
      }
      for (const OutputResult& output : outputResults) {
        if (!assignExternalOutput(root, output.memberIndex, output.value,
                                  "dpi_output_argument")) {
          return false;
        }
      }
    }

    for (const auto& constant : instance.plan.outputConstants) {
      Node* member = root->member[constant.member];
      const LoweredValue& target = nodeValues_.at(member);
      LoweredValue value = emitConstant(target.width, target.sign, constant.literal);
      if (value.symbol.empty() ||
          !assignExternalOutput(root, constant.member, value, "stub_constant")) {
        return false;
      }
    }
    return true;
  }

  bool lowerAndEmitExternalModules() {
    for (ExternalInstance& instance : externalInstances_) {
      if (!lowerAndEmitExternalInstance(instance)) return false;
    }
    return true;
  }

  bool emitRegisterDeclarations() {
    for (Node* src : registerSources_) {
      const std::string state = registerSymbol(src);
      const LoweredValue& value = nodeValues_.at(src);
      if (!emitOperation(state, "kRegister", {}, {},
                         {intAttr("width", value.width),
                          boolAttr("isSigned", value.sign),
                          stringAttr("initValue", std::to_string(value.width) + "'h0"),
                          intAttr("gsim.node_id", src->id),
                          stringAttr("gsim.node_name", src->name)})) {
        return false;
      }
      if (!emitOperation("gsim.reg_read." + std::to_string(src->id),
                         "kRegisterReadPort", {}, {nodeValues_.at(src).symbol},
                         {stringAttr("regSymbol", state)})) {
        return false;
      }
      assignedNodes_.insert(src);
    }
    return true;
  }

  std::optional<LoweredValue> lowerMemoryReadAddress(Node* port) {
    if (!port || !port->parent || !port->memTree || !port->memTree->getRoot()) {
      fail(nodeContext(port) + ": cannot lower memory read address");
      return std::nullopt;
    }
    const ENode* root = port->memTree->getRoot();
    if (root->opType != OP_READ_MEM || root->memoryNode != port->parent ||
        root->child.size() != 1 || !root->child[0]) {
      fail(nodeContext(port) + ": malformed memory read expression");
      return std::nullopt;
    }
    if (port->parent->rlatency == 0) {
      auto address = lowerExpression(port, root->child[0]);
      if (!address) return std::nullopt;
      if (address->isArray() || address->sign || address->width <= 0 ||
          address->width > 64) {
        fail(nodeContext(port) + ": memory read address must be an unsigned scalar");
        return std::nullopt;
      }
      return address;
    }

    const ENode* addressReference = root->child[0];
    Node* addressRegister = addressReference->nodePtr;
    if (!addressRegister || !addressReference->child.empty() ||
        addressRegister->type != NODE_REG_SRC || !addressRegister->regNext ||
        addressRegister->regNext->type != NODE_REG_DST) {
      fail(nodeContext(port) +
           ": synchronous memory read does not reference its generated address register");
      return std::nullopt;
    }
    auto nextAddress = nodeValues_.find(addressRegister->regNext);
    if (nextAddress == nodeValues_.end() || nextAddress->second.isArray() ||
        nextAddress->second.sign || nextAddress->second.width <= 0 ||
        nextAddress->second.width > 64) {
      fail(nodeContext(port) +
           ": generated memory next-address value is unavailable");
      return std::nullopt;
    }
    // GSim's generated address register is the implementation state of a
    // synchronous FIRRTL memory port.  The value sampled on this edge is the
    // register destination, while the register source is the previous request.
    // The executable GRH adds an explicit read-data register, so reading with
    // the source here would accidentally add a second cycle of latency.
    return nextAddress->second;
  }

  bool emitMemoryDeclarationsAndReads() {
    for (Node* memory : memories_) {
      EnodeContextGuard memGuard{enodeContextStack_, std::string("OP_INFER_MEM")};
      const LoweredValue& row = nodeValues_.at(memory);
      if (!emitOperation(memorySymbol(memory), "kMemory", {}, {},
                         {intAttr("width", row.width),
                          intAttr("row", memory->depth),
                          boolAttr("isSigned", row.sign),
                          stringListAttr("initKind", {}),
                          stringListAttr("initFile", {}),
                          stringListAttr("initValue", {}),
                          intListAttr("initStart", {}),
                          intListAttr("initLen", {}),
                          intAttr("gsim.node_id", memory->id),
                          stringAttr("gsim.node_name", memory->name),
                          stringAttr("gsim.read_under_write",
                                     memory->extraInfo.empty() ? "undefined"
                                                               : memory->extraInfo)})) {
        return false;
      }
    }

    for (Node* port : memoryPorts_) {
      if (port->type == NODE_WRITER) continue;
      NodeContextGuard nodeGuard{nodeContextStack_, port};
      EnodeContextGuard readGuard{
          enodeContextStack_,
          enodeContextKey(port->memTree ? port->memTree->getRoot() : nullptr)};
      auto address = lowerMemoryReadAddress(port);
      if (!address) return false;
      Node* memory = port->parent;
      const LoweredValue& value = nodeValues_.at(port);
      if (memory->rlatency == 0) {
        if (!emitOperation("gsim.mem_read." + std::to_string(port->id),
                           "kMemoryReadPort", {address->symbol}, {value.symbol},
                           {stringAttr("memSymbol", memorySymbol(memory)),
                            intAttr("gsim.node_id", port->id)})) {
          return false;
        }
      } else {
        const std::string state = memoryReadRegisterSymbol(port);
        if (!emitOperation(state, "kRegister", {}, {},
                           {intAttr("width", value.width),
                            boolAttr("isSigned", value.sign),
                            stringAttr("initValue", std::to_string(value.width) + "'h0"),
                            intAttr("gsim.node_id", port->id),
                            stringAttr("gsim.node_name", port->name),
                            stringAttr("gsim.role", "synchronous_memory_read")})) {
          return false;
        }
        if (!emitOperation("gsim.mem_read_reg_read." + std::to_string(port->id),
                           "kRegisterReadPort", {}, {value.symbol},
                           {stringAttr("regSymbol", state)})) {
          return false;
        }
        LoweredValue oldData = value;
        oldData.symbol = nextTemporaryValueSymbol();
        if (!emitValue(oldData) ||
            !emitOperation("gsim.mem_read_old." + std::to_string(port->id),
                           "kMemoryReadPort", {address->symbol}, {oldData.symbol},
                           {stringAttr("memSymbol", memorySymbol(memory)),
                            intAttr("gsim.node_id", port->id),
                            stringAttr("gsim.role", "prewrite_read_data")})) {
          return false;
        }
        synchronousMemoryReads_.push_back(
            SynchronousMemoryRead{memory, port, *address, oldData});
      }
      assignedNodes_.insert(port);
    }
    return true;
  }

  bool isConstOneBit(const LoweredValue& value) const {
    auto it = constantValues_.find("1:u:1'h1");
    return it != constantValues_.end() && it->second.symbol == value.symbol;
  }

  std::optional<LoweredValue> lowerMemoryGuard(Node* port, const ENode* context,
                                                const LoweredValue& lhs,
                                                const LoweredValue& rhs) {
    if (lhs.isArray() || rhs.isArray() || lhs.width != 1 || rhs.width != 1) {
      fail(expressionContext(port, context) + ": memory write guard must be 1 bit");
      return std::nullopt;
    }
    if (isConstOneBit(lhs)) return rhs;
    if (isConstOneBit(rhs)) return lhs;
    LoweredValue guard = emitTypedOperation(
        "kAnd", {lhs, rhs}, scalarValue({}, 1, false));
    if (guard.symbol.empty()) return std::nullopt;
    return guard;
  }

  bool appendMemoryWrite(Node* port, const ENode* lvalue,
                         const ENode* expression, LoweredValue condition) {
    Node* memory = port->parent;
    if (!memory || memory->type != NODE_MEMORY) {
      return fail(nodeContext(port) + ": memory write has no backing memory");
    }

    const ENode* addressExpression = nullptr;
    const ENode* dataExpression = expression;
    if (expression->opType == OP_WRITE_MEM) {
      if (expression->memoryNode != memory || expression->child.size() != 2 ||
          !expression->child[0] || !expression->child[1]) {
        return fail(expressionContext(port, expression) + ": malformed OP_WRITE_MEM");
      }
      addressExpression = expression->child[0];
      dataExpression = expression->child[1];
    } else {
      const ENode* root = port->memTree ? port->memTree->getRoot() : nullptr;
      if (!root || root->memoryNode != memory || root->child.size() != 1 ||
          !root->child[0]) {
        return fail(nodeContext(port) + ": memory write is missing its address expression");
      }
      addressExpression = root->child[0];
    }

    auto address = lowerExpression(port, addressExpression);
    auto data = lowerExpression(port, dataExpression);
    if (!address || !data) return false;
    if (address->isArray() || address->sign || address->width <= 0 ||
        address->width > 64) {
      return fail(expressionContext(port, addressExpression) +
                  ": memory write address must be an unsigned scalar");
    }

    const LoweredValue& portShape = nodeValues_.at(port);
    const LoweredValue& rowShape = nodeValues_.at(memory);
    auto path = lowerIndexPath(port, port, lvalue, portShape);
    if (!path) return false;
    LoweredValue zero = emitConstant(
        rowShape.width, false, std::to_string(rowShape.width) + "'h0");
    if (zero.symbol.empty()) return false;
    zero.elementWidth = rowShape.elementWidth;
    zero.elementSign = rowShape.elementSign;
    zero.dimensions = rowShape.dimensions;
    auto packedData = insertIndexedValue(port, lvalue, zero, *path, *data);
    if (!packedData) return false;

    LoweredValue mask;
    if (path->staticBitOffset) {
      const int64_t low = *path->staticBitOffset;
      const int64_t high = low + path->selectionShape.width - 1;
      if (high >= rowShape.width) {
        return fail(expressionContext(port, lvalue) +
                    ": indexed memory write exceeds packed row width");
      }
      mask = emitRangeOnesConstant(rowShape.width, static_cast<int>(low),
                                   static_cast<int>(high));
    } else {
      LoweredValue baseMask = emitRangeOnesConstant(
          rowShape.width, 0, path->selectionShape.width - 1);
      if (baseMask.symbol.empty()) return false;
      mask = emitTypedOperation(
          "kShl", {baseMask, path->dynamicBitOffset},
          scalarValue({}, rowShape.width, false));
      if (mask.symbol.empty()) return false;
      if (!path->inRange.symbol.empty()) {
        auto guarded = lowerMemoryGuard(port, lvalue, condition, path->inRange);
        if (!guarded) return false;
        condition = *guarded;
      }
    }
    if (mask.symbol.empty()) return false;

    memoryWrites_.push_back(LoweredMemoryWrite{
        memory, port, expression, condition, *address, *packedData, mask});
    return true;
  }

  /* Vector registers (array-typed REG_DST) keep their OP_WHEN skeletons
     (flattenNodes skips arrays), so their conditional partial updates can be
     exported as one masked write port per when leaf — the same effect-style
     form the memory write path already uses — instead of a functional
     full-value merge mux chain feeding a single all-ones-mask write.
     Leaves are collected in gsim source order (assignTree order, then-branch
     before else-branch; the else guard carries the negated condition, so
     same-tree leaves are mutually exclusive and cross-tree last-win is just
     emission order). */

  bool usePerLeafRegisterWrites(const Node* dst) const {
    auto it = nodeValues_.find(dst);
    return it != nodeValues_.end() && it->second.isArray();
  }

  bool appendRegisterWrite(Node* src, Node* dst, const ENode* lvalue,
                           const ENode* expression, LoweredValue condition,
                           std::vector<LoweredRegisterWrite>& writes) {
    const LoweredValue& registerShape = nodeValues_.at(dst);
    auto data = lowerExpression(dst, expression);
    if (!data) return false;
    auto path = lowerIndexPath(dst, dst, lvalue, registerShape);
    if (!path) return false;
    LoweredValue zero = emitConstant(
        registerShape.width, false, std::to_string(registerShape.width) + "'h0");
    if (zero.symbol.empty()) return false;
    zero.elementWidth = registerShape.elementWidth;
    zero.elementSign = registerShape.elementSign;
    zero.dimensions = registerShape.dimensions;
    auto packedData = insertIndexedValue(dst, lvalue, zero, *path, *data);
    if (!packedData) return false;

    LoweredValue mask;
    if (path->staticBitOffset) {
      const int64_t low = *path->staticBitOffset;
      const int64_t high = low + path->selectionShape.width - 1;
      if (high >= registerShape.width) {
        return fail(expressionContext(dst, lvalue) +
                    ": indexed register update exceeds packed register width");
      }
      mask = emitRangeOnesConstant(registerShape.width, static_cast<int>(low),
                                   static_cast<int>(high));
    } else {
      LoweredValue baseMask = emitRangeOnesConstant(
          registerShape.width, 0, path->selectionShape.width - 1);
      if (baseMask.symbol.empty()) return false;
      mask = emitTypedOperation("kShl", {baseMask, path->dynamicBitOffset},
                                scalarValue({}, registerShape.width, false));
      if (mask.symbol.empty()) return false;
      if (!path->inRange.symbol.empty()) {
        auto guarded = lowerMemoryGuard(dst, lvalue, condition, path->inRange);
        if (!guarded) return false;
        condition = *guarded;
      }
    }
    if (mask.symbol.empty()) return false;
    writes.push_back(
        LoweredRegisterWrite{src, expression, condition, *packedData, mask, false});
    return true;
  }

  bool lowerRegisterWriteLeaves(Node* src, Node* dst, const ENode* lvalue,
                                const ENode* expression,
                                const LoweredValue& condition,
                                std::vector<LoweredRegisterWrite>& writes) {
    if (!expression) return true;
    if (!expression->nodePtr &&
        (expression->opType == OP_INVALID || expression->opType == OP_EMPTY)) {
      /* missing/invalid when branch: the register holds, i.e. no write */
      return true;
    }
    EnodeContextGuard writeGuard{enodeContextStack_, enodeContextKey(expression)};
    enodeVisitCounts_[enodeContextStack_.back()]++;
    if (expression->opType != OP_WHEN) {
      return appendRegisterWrite(src, dst, lvalue, expression, condition, writes);
    }
    if (expression->child.size() != 3 || !expression->child[0]) {
      return fail(expressionContext(dst, expression) +
                  ": malformed conditional register update");
    }
    auto branchCondition = lowerExpression(dst, expression->child[0]);
    if (!branchCondition) return false;
    if (branchCondition->isArray() || branchCondition->width != 1) {
      return fail(expressionContext(dst, expression) +
                  ": conditional register update guard must be 1 bit");
    }
    if (expression->child[1]) {
      auto trueCondition = lowerMemoryGuard(
          dst, expression, condition, *branchCondition);
      if (!trueCondition ||
          !lowerRegisterWriteLeaves(src, dst, lvalue, expression->child[1],
                                    *trueCondition, writes)) {
        return false;
      }
    }
    if (expression->child[2]) {
      LoweredValue inverse = emitTypedOperation(
          "kNot", {*branchCondition}, scalarValue({}, 1, false));
      if (inverse.symbol.empty()) return false;
      auto falseCondition = lowerMemoryGuard(dst, expression, condition, inverse);
      if (!falseCondition ||
          !lowerRegisterWriteLeaves(src, dst, lvalue, expression->child[2],
                                    *falseCondition, writes)) {
        return false;
      }
    }
    return true;
  }

  /* the reset write becomes the last write leaf (all-ones mask), so it wins
     over every normal update of the same cycle by emission order; an async
     reset additionally rides the reset-signal posedge as a second event */
  bool appendRegisterSyncReset(Node* src, Node* dst,
                               std::vector<LoweredRegisterWrite>& writes) {
    if (src->reset == ZERO_RESET) return true;
    if ((src->reset != UINTRESET && src->reset != ASYRESET) || !src->resetTree) {
      return fail(nodeContext(src) + ": unsupported reset representation");
    }
    if (!validateLvalue(src, src->resetTree, 0)) return false;
    if (!src->resetTree->getlval()->child.empty()) {
      return fail(nodeContext(src) + ": indexed array reset lvalues are not supported");
    }
    const ENode* root = src->resetTree->getRoot();
    if (!root || root->opType != OP_RESET || !expectChildren(src, root, 2, false)) {
      return fail(nodeContext(src) + ": resetTree root must be OP_RESET(cond, value)");
    }
    EnodeContextGuard resetGuard{enodeContextStack_, enodeContextKey(root)};
    enodeVisitCounts_[enodeContextStack_.back()]++;
    auto cond = lowerExpression(src, root->child[0]);
    auto value = lowerExpression(src, root->child[1]);
    if (!cond || !value) return false;
    if (cond->isArray() || cond->width != 1) {
      return fail(nodeContext(src) + ": reset condition must be scalar one-bit logic");
    }
    const LoweredValue& registerShape = nodeValues_.at(dst);
    if (value->isArray()) {
      if (value->dimensions != registerShape.dimensions) {
        return fail(nodeContext(src) +
                    ": reset value packed-array dimensions do not exactly match the register");
      }
    } else if (root->child[1]->opType != OP_INT) {
      return fail(nodeContext(src) +
                  ": only GSim's scalar constant canonical form may broadcast to an array reset");
    }
    auto resetValue = coerceToShape(src, root->child[1], *value, registerShape);
    if (!resetValue) return false;
    LoweredValue mask = emitAllOnesConstant(registerShape.width);
    if (mask.symbol.empty()) return false;
    LoweredRegisterWrite write{src, root, *cond, *resetValue, mask,
                               src->reset == UINTRESET};
    if (src->reset == ASYRESET) write.asyncEvent = *cond;
    writes.push_back(std::move(write));
    return true;
  }

  bool lowerMemoryWriteExpression(Node* port, const ENode* lvalue,
                                  const ENode* expression,
                                  const LoweredValue& condition) {
    if (!expression) return true;
    if (expression->opType == OP_INVALID || expression->opType == OP_READ_MEM) {
      return true;
    }
    EnodeContextGuard writeGuard{enodeContextStack_, enodeContextKey(expression)};
    enodeVisitCounts_[enodeContextStack_.back()]++;
    if (expression->opType != OP_WHEN) {
      return appendMemoryWrite(port, lvalue, expression, condition);
    }
    if (expression->child.size() != 3 || !expression->child[0]) {
      return fail(expressionContext(port, expression) +
                  ": malformed conditional memory write");
    }
    auto branchCondition = lowerExpression(port, expression->child[0]);
    if (!branchCondition) return false;
    if (branchCondition->isArray() || branchCondition->width != 1) {
      return fail(expressionContext(port, expression) +
                  ": conditional memory write guard must be 1 bit");
    }
    if (expression->child[1]) {
      auto trueCondition = lowerMemoryGuard(
          port, expression, condition, *branchCondition);
      if (!trueCondition ||
          !lowerMemoryWriteExpression(port, lvalue, expression->child[1],
                                      *trueCondition)) {
        return false;
      }
    }
    if (expression->child[2]) {
      LoweredValue inverse = emitTypedOperation(
          "kNot", {*branchCondition}, scalarValue({}, 1, false));
      if (inverse.symbol.empty()) return false;
      auto falseCondition = lowerMemoryGuard(port, expression, condition, inverse);
      if (!falseCondition ||
          !lowerMemoryWriteExpression(port, lvalue, expression->child[2],
                                      *falseCondition)) {
        return false;
      }
    }
    return true;
  }

  bool hasMemoryWriteAction(const ENode* expression) const {
    if (!expression || expression->opType == OP_INVALID ||
        expression->opType == OP_READ_MEM) {
      return false;
    }
    if (expression->opType != OP_WHEN) return true;
    if (expression->child.size() != 3) return true;
    return hasMemoryWriteAction(expression->child[1]) ||
           hasMemoryWriteAction(expression->child[2]);
  }

  bool lowerAndEmitMemoryWrites() {
    LoweredValue active = emitConstant(1, false, "1'h1");
    if (active.symbol.empty()) return false;
    for (Node* memory : memories_) {
      for (Node* port : memory->member) {
        if (port->type == NODE_READER) continue;
        NodeContextGuard nodeGuard{nodeContextStack_, port};
        const size_t firstWrite = memoryWrites_.size();
        bool hasWriteAction = false;
        for (size_t treeIndex = 0; treeIndex < port->assignTree.size(); treeIndex++) {
          ExpTree* tree = port->assignTree[treeIndex];
          if (!validateLvalue(port, tree, treeIndex)) return false;
          hasWriteAction |= hasMemoryWriteAction(tree->getRoot());
          if (!lowerMemoryWriteExpression(port, tree->getlval(), tree->getRoot(), active)) {
            return false;
          }
        }
        // ConstantAnalysis removes a conditional writer's complete assignTree
        // when every write guard is false.  The backing memory and this inert
        // port can remain live because another port still reads the memory.
        if (memoryWrites_.size() == firstWrite && hasWriteAction) {
          return fail(nodeContext(port) + ": live writer has no executable write action");
        }
      }
    }

    using WriteGroupKey = std::pair<int, int>;
    std::map<WriteGroupKey, size_t> groupCounts;
    for (const LoweredMemoryWrite& write : memoryWrites_) {
      groupCounts[{write.memory->id, write.port->clock->id}]++;
    }
    std::map<WriteGroupKey, size_t> groupOffsets;
    for (const LoweredMemoryWrite& write : memoryWrites_) {
      NodeContextGuard nodeGuard{nodeContextStack_, write.port};
      EnodeContextGuard writeEmitGuard{enodeContextStack_,
                                       enodeContextKey(write.expression)};
      auto clock = nodeValues_.find(write.port->clock);
      if (clock == nodeValues_.end() || clock->second.width != 1 ||
          clock->second.isArray()) {
        return fail(nodeContext(write.port) + ": memory write clock is unavailable");
      }
      const WriteGroupKey key{write.memory->id, write.port->clock->id};
      const size_t offset = groupOffsets[key]++;
      const int64_t priority = static_cast<int64_t>(groupCounts.at(key) - offset - 1);
      const std::string group = "gsim.mem_write_group." +
                                std::to_string(key.first) + "." +
                                std::to_string(key.second);
      if (!emitOperation("gsim.mem_write." +
                             std::to_string(nextMemoryWriteOperationId_++),
                         "kMemoryWritePort",
                         {write.condition.symbol, write.address.symbol,
                          write.data.symbol, write.mask.symbol,
                          clock->second.symbol}, {},
                         {stringAttr("memSymbol", memorySymbol(write.memory)),
                          stringListAttr("eventEdge", {"posedge"}),
                          stringAttr("memoryWrite.priorityGroup", group),
                          intAttr("memoryWrite.priority", priority),
                          intAttr("gsim.node_id", write.port->id)})) {
        return false;
      }
    }
    return true;
  }

  bool emitSynchronousMemoryReadWrites() {
    LoweredValue active = emitConstant(1, false, "1'h1");
    if (active.symbol.empty()) return false;
    for (const SynchronousMemoryRead& read : synchronousMemoryReads_) {
      NodeContextGuard nodeGuard{nodeContextStack_, read.port};
      LoweredValue data = read.oldData;
      const std::string ruw = read.memory->extraInfo.empty()
                                  ? "undefined" : read.memory->extraInfo;
      if (ruw == "new") {
        for (const LoweredMemoryWrite& write : memoryWrites_) {
          if (write.memory != read.memory || write.port->clock != read.port->clock) {
            continue;
          }
          EnodeContextGuard ruwGuard{enodeContextStack_,
                                     enodeContextKey(write.expression)};
          const int compareWidth = std::max(read.address.width, write.address.width);
          const LoweredValue compareShape = scalarValue({}, compareWidth, false);
          auto readAddress = coerceToShape(
              read.port, write.expression, read.address, compareShape);
          auto writeAddress = coerceToShape(
              read.port, write.expression, write.address, compareShape);
          if (!readAddress || !writeAddress) return false;
          LoweredValue sameAddress = emitTypedOperation(
              "kEq", {*readAddress, *writeAddress}, scalarValue({}, 1, false));
          if (sameAddress.symbol.empty()) return false;
          auto applies = lowerMemoryGuard(
              read.port, write.expression, write.condition, sameAddress);
          if (!applies) return false;

          LoweredValue inverseMask = emitTypedOperation(
              "kNot", {write.mask}, scalarValue({}, data.width, false));
          LoweredValue preserved = emitTypedOperation(
              "kAnd", {data, inverseMask}, data);
          LoweredValue replacement = emitTypedOperation(
              "kAnd", {write.data, write.mask}, data);
          LoweredValue merged = emitTypedOperation(
              "kOr", {preserved, replacement}, data);
          if (inverseMask.symbol.empty() || preserved.symbol.empty() ||
              replacement.symbol.empty() || merged.symbol.empty()) {
            return false;
          }
          data = emitTypedOperation("kMux", {*applies, merged, data}, data);
          if (data.symbol.empty()) return false;
        }
      }

      LoweredValue mask = emitAllOnesConstant(data.width);
      auto clock = nodeValues_.find(read.port->clock);
      if (mask.symbol.empty() || clock == nodeValues_.end() ||
          clock->second.width != 1 || clock->second.isArray()) {
        return fail(nodeContext(read.port) +
                    ": synchronous memory read clock is unavailable");
      }
      if (!emitOperation("gsim.mem_read_reg_write." +
                             std::to_string(read.port->id),
                         "kRegisterWritePort",
                         {active.symbol, data.symbol, mask.symbol,
                          clock->second.symbol}, {},
                         {stringAttr("regSymbol",
                                     memoryReadRegisterSymbol(read.port)),
                          stringListAttr("eventEdge", {"posedge"}),
                          intAttr("gsim.node_id", read.port->id),
                          stringAttr("gsim.read_under_write", ruw)})) {
        return false;
      }
    }
    return true;
  }

  std::optional<LoweredRegisterUpdate> lowerRegisterReset(
      Node* src, LoweredValue normalData) {
    if (src->reset == ZERO_RESET) {
      return LoweredRegisterUpdate{std::move(normalData), std::nullopt};
    }
    if ((src->reset != UINTRESET && src->reset != ASYRESET) || !src->resetTree) {
      fail(nodeContext(src) + ": unsupported reset representation");
      return std::nullopt;
    }
    if (!validateLvalue(src, src->resetTree, 0)) return std::nullopt;
    if (!src->resetTree->getlval()->child.empty()) {
      fail(nodeContext(src) + ": indexed array reset lvalues are not supported");
      return std::nullopt;
    }
    const ENode* root = src->resetTree->getRoot();
    if (!root || root->opType != OP_RESET || !expectChildren(src, root, 2, false)) {
      fail(nodeContext(src) + ": resetTree root must be OP_RESET(cond, value)");
      return std::nullopt;
    }
    EnodeContextGuard resetGuard{enodeContextStack_, enodeContextKey(root)};
    enodeVisitCounts_[enodeContextStack_.back()]++;
    auto cond = lowerExpression(src, root->child[0]);
    auto value = lowerExpression(src, root->child[1]);
    if (!cond || !value) return std::nullopt;
    if (cond->isArray() || cond->width != 1) {
      fail(nodeContext(src) + ": reset condition must be scalar one-bit logic");
      return std::nullopt;
    }
    if (normalData.isArray()) {
      if (value->isArray()) {
        if (value->dimensions != normalData.dimensions) {
          fail(nodeContext(src) +
               ": reset value packed-array dimensions do not exactly match the register");
          return std::nullopt;
        }
      } else if (root->child[1]->opType != OP_INT) {
        fail(nodeContext(src) +
             ": only GSim's scalar constant canonical form may broadcast to an array reset");
        return std::nullopt;
      }
    } else if (value->isArray()) {
      fail(nodeContext(src) + ": array reset value cannot target a scalar register");
      return std::nullopt;
    }
    auto resetValue = coerceToShape(src, root->child[1], *value, normalData);
    if (!resetValue) return std::nullopt;
    LoweredValue result = emitTypedOperation(
        "kMux", {*cond, *resetValue, normalData}, normalData,
        {stringAttr("gsim.role", "register_reset")});
    if (result.symbol.empty()) return std::nullopt;
    LoweredRegisterUpdate update;
    update.data = std::move(result);
    if (src->reset == ASYRESET) update.asyncResetCondition = *cond;
    return update;
  }

  bool emitRegisterWrites() {
    LoweredValue updateCond = emitConstant(1, false, "1'h1");
    if (updateCond.symbol.empty()) return false;
    for (Node* src : registerSources_) {
      Node* dst = src->regNext;
      if (!assignedNodes_.count(dst)) {
        return fail(nodeContext(dst) + ": register destination value was not lowered");
      }
      auto clockIt = nodeValues_.find(src->clock);
      if (clockIt == nodeValues_.end() || clockIt->second.isArray() ||
          clockIt->second.width != 1) {
        return fail(nodeContext(src) + ": clock is unavailable or not scalar 1 bit");
      }
      if (usePerLeafRegisterWrites(dst)) {
        std::vector<LoweredRegisterWrite> writes;
        {
          NodeContextGuard nodeGuard{nodeContextStack_, dst};
          for (size_t i = 0; i < dst->assignTree.size(); i++) {
            ExpTree* tree = dst->assignTree[i];
            if (!validateLvalue(dst, tree, i)) return false;
            if (!lowerRegisterWriteLeaves(src, dst, tree->getlval(),
                                          tree->getRoot(), updateCond, writes)) {
              return false;
            }
          }
          if (!appendRegisterSyncReset(src, dst, writes)) return false;
        }
        NodeContextGuard nodeGuard{nodeContextStack_, src};
        size_t leafIndex = 0;
        for (const LoweredRegisterWrite& write : writes) {
          std::vector<std::string> inputs = {write.condition.symbol, write.data.symbol,
                                             write.mask.symbol, clockIt->second.symbol};
          std::vector<std::string> eventEdges = {"posedge"};
          if (write.asyncEvent) {
            inputs.push_back(write.asyncEvent->symbol);
            eventEdges.push_back("posedge");
          }
          if (!emitOperation(
                  "gsim.reg_write." + std::to_string(src->id) + "." +
                      std::to_string(leafIndex++),
                  "kRegisterWritePort", inputs, {},
                  {stringAttr("regSymbol", registerSymbol(src)),
                   stringListAttr("eventEdge", std::move(eventEdges)),
                   stringAttr("gsim.reset_kind",
                              write.isSyncReset ? "sync"
                                                : (write.asyncEvent ? "async" : "none")),
                   intAttr("gsim.node_id", src->id)})) {
            return false;
          }
        }
        continue;
      }
      NodeContextGuard nodeGuard{nodeContextStack_, src};
      auto update = lowerRegisterReset(src, nodeValues_.at(dst));
      if (!update) return false;
      LoweredValue mask = emitAllOnesConstant(nodeValues_.at(src).width);
      if (mask.symbol.empty()) return false;
      std::vector<std::string> inputs = {
          updateCond.symbol, update->data.symbol, mask.symbol, clockIt->second.symbol};
      std::vector<std::string> eventEdges = {"posedge"};
      if (update->asyncResetCondition) {
        inputs.push_back(update->asyncResetCondition->symbol);
        eventEdges.push_back("posedge");
      }
      if (!emitOperation(
              "gsim.reg_write." + std::to_string(src->id),
              "kRegisterWritePort", inputs, {},
              [&]() {
                std::vector<JsonAttr> attributes = {
                    stringAttr("regSymbol", registerSymbol(src)),
                    stringListAttr("eventEdge", std::move(eventEdges)),
                    stringAttr("gsim.reset_kind",
                               src->reset == ASYRESET ? "async" :
                               src->reset == UINTRESET ? "sync" : "none"),
                    intAttr("gsim.node_id", src->id)};
                if (constantRegisterDestinations_.count(dst)) {
                  attributes.push_back(
                      boolAttr("gsim.constant_normal_update", true));
                }
                return attributes;
              }())) {
        return false;
      }
    }
    return true;
  }

  bool emitModel() {
    for (Node* node : semanticNodes_) {
      if (node->type == NODE_SPECIAL || node->type == NODE_EXT) continue;
      LoweredValue value = nodeValue(node);
      nodeValues_[node] = value;
      if (node->type != NODE_MEMORY && node->type != NODE_WRITER &&
          !emitValue(value, inputNodes_.count(node), outputNodes_.count(node), node)) {
        return false;
      }
    }
    if (!emitRegisterDeclarations()) return false;
    if (!emitMemoryDeclarationsAndReads()) return false;

    for (Node* node : semanticNodes_) {
      switch (node->type) {
        case NODE_INP:
        case NODE_REG_SRC:
          assignedNodes_.insert(node);
          break;
        case NODE_OUT:
        case NODE_OTHERS:
        case NODE_REG_DST:
        case NODE_EXT_IN:
          if (!lowerAssignedNode(node)) return false;
          break;
        case NODE_READER:
        case NODE_READWRITER:
          if (!assignedNodes_.count(node)) {
            return fail(nodeContext(node) + ": memory read value was not lowered");
          }
          break;
        case NODE_MEMORY:
        case NODE_WRITER:
        case NODE_EXT:
        case NODE_EXT_OUT:
          break;
        case NODE_SPECIAL:
          if (!optimizerElidedEffects_.count(node) && !lowerAndEmitEffect(node)) return false;
          break;
        case NODE_INFER:
        case NODE_REG_RESET:
        case NODE_INVALID:
          return fail(nodeContext(node) + ": unsupported node reached lowering");
      }
    }
    if (!lowerAndEmitExternalModules()) return false;
    if (!lowerAndEmitMemoryWrites()) return false;
    if (!emitSynchronousMemoryReadWrites()) return false;
    if (!emitRegisterWrites()) return false;

    for (const ExternalInstance& instance : externalInstances_) {
      for (Node* member : instance.root->member) {
        if (member->type == NODE_EXT_OUT && !assignedNodes_.count(member)) {
          return fail(nodeContext(member) +
                      ": external output has no executable definition");
        }
      }
    }

    for (Node* output : source_->output) {
      if (!assignedNodes_.count(output)) {
        return fail(nodeContext(output) + ": output has no executable definition");
      }
    }
    return static_cast<bool>(values_) && static_cast<bool>(ops_);
  }

  bool appendFile(std::ostream& output, const std::string& path) {
    std::ifstream input(path, std::ios::in);
    if (!input.is_open()) return fail("cannot reopen spool '" + path + "'");
    output << input.rdbuf();
    if (!input.eof() && input.fail()) return fail("failed reading spool '" + path + "'");
    return static_cast<bool>(output);
  }

  void writePorts(std::ostream& os, const std::vector<Node*>& ports) {
    for (size_t i = 0; i < ports.size(); i++) {
      if (i) os << ", ";
      os << "{\"name\": ";
      writeJsonString(os, ports[i]->name);
      os << ", \"val\": ";
      writeJsonString(os, nodeValues_.at(ports[i]).symbol);
      os << "}";
    }
  }

  bool assemble() {
    std::ofstream output(assembledPath_, std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
      return fail("cannot open assembled output '" + assembledPath_ + "': " +
                  std::strerror(errno));
    }
    size_t externalCallCount = 0;
    for (const ExternalInstance& instance : externalInstances_) {
      externalCallCount += instance.plan.calls.size();
    }

    output << "{\n  \"format\": ";
    writeJsonString(output, kExecutableGrhFormat);
    output << ",\n  \"stage\": \"pre-coarsen\",\n"
              "  \"boundary\": \"PreCoarsen\",\n"
              "  \"analysisOnly\": false,\n"
              "  \"executionProfile\": ";
    writeJsonString(output, externalProfileName_);
    output << ",\n  \"gsim\": {\"format\": ";
    writeJsonString(output, kExecutableGrhFormat);
    output << ", \"stage\": \"pre-coarsen\", \"boundary\": \"PreCoarsen\", "
              "\"analysisOnly\": false, \"version\": ";
    writeJsonString(output, GSIM_VERSION);
    output << ", \"buildDate\": ";
    writeJsonString(output, GSIM_BUILD_DATE);
    output << ", \"inputFile\": ";
    writeJsonString(output, globalConfig.InputFile);
    output << ", \"inputFileBytes\": " << globalConfig.InputFileBytes
           << ", \"executionProfile\": ";
    writeJsonString(output, externalProfileName_);
    output << ", \"nodeCount\": " << semanticNodes_.size()
           << ", \"valueCount\": " << emittedValueSymbols_.size()
           << ", \"operationCount\": " << emittedOperationSymbols_.size()
           << ", \"nodeFinalAssignElidedCount\": " << nodeFinalAssignElidedCount_
           << ", \"nodeFinalAssignKeptCount\": " << nodeFinalAssignKeptCount_
           << ", \"externalInstanceCount\": " << externalInstances_.size()
           << ", \"externalCallCount\": " << externalCallCount
           << ", \"dpiImportCount\": " << emittedDpiImportSignatures_.size()
           << "},\n"
              "  \"graphs\": [\n    {\n      \"symbol\": ";
    writeJsonString(output, source_->name);
    output << ",\n      \"attrs\": {"
              "\"gsim.format\": {\"t\": \"string\", \"v\": ";
    writeJsonString(output, kExecutableGrhFormat);
    output << "}, \"gsim.stage\": {\"t\": \"string\", \"v\": \"pre-coarsen\"}, "
              "\"gsim.boundary\": {\"t\": \"string\", \"v\": \"PreCoarsen\"}, "
              "\"gsim.execution_profile\": {\"t\": \"string\", \"v\": ";
    writeJsonString(output, externalProfileName_);
    output << "}},\n"
              "      \"declaredSymbols\": [],\n"
              "      \"vals\": [\n";
    if (!appendFile(output, valuesPath_)) return false;
    output << "\n      ],\n      \"ports\": {\n        \"in\": [";
    writePorts(output, source_->input);
    output << "],\n        \"out\": [";
    writePorts(output, source_->output);
    output << "],\n        \"inout\": []\n      },\n      \"ops\": [\n";
    if (!appendFile(output, opsPath_)) return false;
    output << "\n      ]\n    }\n  ],\n  \"aliases\": {";
    writeJsonString(output, source_->name);
    output << ": ";
    writeJsonString(output, source_->name);
    output << "},\n  \"declaredSymbols\": [";
    writeJsonString(output, source_->name);
    output << "],\n  \"tops\": [";
    writeJsonString(output, source_->name);
    output << "]\n}\n";
    output.close();
    if (!output) return fail("failed writing assembled executable GRH JSON");
    if (std::rename(assembledPath_.c_str(), outputPath_.c_str()) != 0) {
      return fail("cannot install executable GRH output '" + outputPath_ + "': " +
                  std::strerror(errno));
    }
    return true;
  }

  void dumpEnodeAttribution() const {
    const std::string path = outputPath_ + ".enode_matrix.json";
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;
    out << "{\n  \"visits\": {";
    bool first = true;
    for (const auto& entry : enodeVisitCounts_) {
      if (!first) out << ", ";
      first = false;
      writeJsonString(out, entry.first);
      out << ": " << entry.second;
    }
    out << "},\n  \"ops\": {";
    first = true;
    for (const auto& entry : enodeOpAttribution_) {
      if (!first) out << ", ";
      first = false;
      writeJsonString(out, entry.first);
      out << ": {";
      bool firstKind = true;
      for (const auto& kindEntry : entry.second) {
        if (!firstKind) out << ", ";
        firstKind = false;
        writeJsonString(out, kindEntry.first);
        out << ": " << kindEntry.second;
      }
      out << "}";
    }
    out << "},\n  \"unownedOps\": {";
    first = true;
    for (const auto& entry : unownedOpCounts_) {
      if (!first) out << ", ";
      first = false;
      writeJsonString(out, entry.first);
      out << ": " << entry.second;
    }
    out << "}\n}\n";
  }

  graph* source_ = nullptr;
  std::string outputPath_;
  std::string valuesPath_;
  std::string opsPath_;
  std::string assembledPath_;
  std::string error_;
  std::ofstream values_;
  std::ofstream ops_;
  bool firstValue_ = true;
  bool firstOperation_ = true;
  size_t nextValueId_ = 0;
  size_t nextOperationId_ = 0;
  size_t nextMemoryWriteOperationId_ = 0;
  std::vector<Node*> semanticNodes_;
  std::unordered_set<Node*> semanticNodeSet_;
  std::unordered_map<int, Node*> nodeById_;
  std::unordered_set<const Node*> inputNodes_;
  std::unordered_set<const Node*> outputNodes_;
  std::unordered_set<const Node*> constantOutputNodes_;
  std::vector<Node*> registerSources_;
  std::vector<Node*> memories_;
  std::vector<Node*> memoryPorts_;
  std::vector<LoweredMemoryWrite> memoryWrites_;
  std::vector<SynchronousMemoryRead> synchronousMemoryReads_;
  std::unordered_set<Node*> registerSourceSet_;
  std::unordered_map<Node*, Node*> registerDestinations_;
  std::unordered_set<const Node*> constantRegisterDestinations_;
  std::unordered_map<const Node*, int> nodePackedWidths_;
  std::unordered_map<const Node*, LoweredValue> nodeValues_;
  std::unordered_map<std::string, LoweredValue> constantValues_;
  std::unordered_map<std::string, std::string> stringConstantValues_;
  std::unordered_map<const Node*, executable_grh::EffectPlan> effectPlans_;
  std::unordered_set<const Node*> optimizerElidedEffects_;
  std::vector<ExternalInstance> externalInstances_;
  std::unordered_map<const Node*, size_t> externalInstanceByRoot_;
  std::unordered_map<const Node*, const Node*> externalClockOwners_;
  executable_grh::ExternalModuleExecutionProfile externalProfile_ =
      executable_grh::ExternalModuleExecutionProfile::FullFidelity;
  std::string externalProfileName_ = "full-fidelity";
  bool externalProfileValid_ = true;
  std::unordered_map<std::string, std::string> emittedDpiImportSignatures_;
  std::unordered_set<const ENode*> activeExpressions_;
  std::vector<std::string> enodeContextStack_;
  std::vector<const Node*> nodeContextStack_;
  std::map<std::string, uint64_t> unownedOpCounts_;
  std::map<std::string, std::map<std::string, uint64_t>> enodeOpAttribution_;
  std::map<std::string, uint64_t> enodeVisitCounts_;
  std::unordered_set<const Node*> assignedNodes_;
  std::unordered_set<std::string> emittedValueSymbols_;
  std::unordered_set<std::string> emittedOperationSymbols_;
  std::optional<NodeEmissionCapture> nodeEmissionCapture_;
  size_t nodeFinalAssignElidedCount_ = 0;
  size_t nodeFinalAssignKeptCount_ = 0;
};

}  // namespace

bool graph::exportExecutableGrh(const std::string& path, std::string& error) {
  ExecutableGrhExporter exporter(this, path);
  return exporter.run(error);
}
