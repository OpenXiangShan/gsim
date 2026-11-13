#include "common.h"
#include "NodeEvolution.h"

#include <filesystem>
#include <iomanip>
#include <numeric>
#include <stdexcept>
#include <unordered_set>

namespace {

std::string escape(const std::string& text) {
  std::string result;
  for (unsigned char ch : text) {
    switch (ch) {
      case '\\': result += "\\\\"; break;
      case '"': result += "\\\""; break;
      case '\n': result += "\\n"; break;
      case '\r': result += "\\r"; break;
      case '\t': result += "\\t"; break;
      default:
        if (ch < 0x20) {
          char escaped[7];
          snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
          result += escaped;
        } else result += ch;
    }
  }
  return result;
}

void integers(std::ostream& out, const std::vector<int>& values) {
  out << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  out << ']';
}

void reference(std::ostream& out, int id) {
  out << "{\"kind\":\"gsim-node\",\"id\":" << id << ",\"generation\":0}";
}

void reference(std::ostream& out, const Node* node) { reference(out, node->id); }

void references(std::ostream& out, const std::vector<int>& nodes) {
  out << '[';
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (i) out << ',';
    reference(out, nodes[i]);
  }
  out << ']';
}

// Delimited native expression serialization, hashed in preorder with explicit
// null children. Node references use stable IDs; allocator addresses and ENode
// allocation IDs do not contribute. No derived prev/next graph edges are used.
class StructureFingerprint {
 public:
  void number(uint64_t value) {
    for (int i = 0; i < 8; ++i) {
      first = (first ^ (value & 255)) * UINT64_C(1099511628211);
      second = (second ^ (value & 255)) * UINT64_C(14029467366897019727);
      value >>= 8;
    }
  }
  void text(const std::string& text) {
    number(text.size());
    for (unsigned char ch : text) number(ch);
  }
  void expression(const ENode* root) {
    std::vector<const ENode*> pending{root};
    while (!pending.empty()) {
      const ENode* node = pending.back();
      pending.pop_back();
      number(node != nullptr);
      if (!node) continue;
      number(node->opType);
      number(node->width);
      number(node->sign);
      number(node->isClock);
      number(node->reset);
      number(node->nodePtr ? node->nodePtr->id : 0);
      number(node->memoryNode ? node->memoryNode->id : 0);
      text(node->strVal);
      number(node->values.size());
      for (int value : node->values) number(value);
      number(node->child.size());
      pending.insert(pending.end(), node->child.rbegin(), node->child.rend());
    }
  }
  void tree(const ExpTree* tree) {
    number(tree != nullptr);
    if (tree) {
      expression(tree->getlval());
      expression(tree->getRoot());
    }
  }
  std::string node(const Node* node) {
    number(node->assignTree.size());
    for (const ExpTree* tree : node->assignTree) this->tree(tree);
    tree(node->resetTree);
    number(node->clock ? node->clock->id : 0);
    number(node->regNext ? node->regNext->id : 0);
    number(node->parent ? node->parent->id : 0);
    number(node->reset);
    number(node->isClock);
    if (node->type == NODE_MEMORY) {
      number(node->depth);
      number(node->rlatency);
      number(node->wlatency);
      text(node->extraInfo);
    }
    std::ostringstream result;
    result << std::hex << std::setfill('0') << std::setw(16) << first << std::setw(16) << second;
    return result.str();
  }
 private:
  uint64_t first = UINT64_C(14695981039346656037);
  uint64_t second = UINT64_C(7809847782465536322);
};

const char* sourceRole(const Node* node) {
  if (node->sourceKind == Node::SourceKind::WhenCondition) return "predicate";
  if (node->sourceKind == Node::SourceKind::MemoryReadAddress) return "memory-read-address";
  switch (node->type) {
    case NODE_SPECIAL: return "effect";
    case NODE_MEMORY: return "memory-storage";
    case NODE_REG_SRC: return "register-current";
    case NODE_REG_DST: return "register-next";
    default: return "value";
  }
}

using Fields = std::vector<std::pair<std::string, std::string>>;

void nullableReference(std::ostream& out, const Node* node) {
  if (node) reference(out, node);
  else out << "null";
}

void expressionReference(std::ostream& out, const ENode* expression) {
  if (expression) out << expression->id;
  else out << "null";
}

void expressionTree(std::ostream& out, const ExpTree* tree) {
  if (!tree) { out << "null"; return; }
  out << "{\"lvalue\":";
  expressionReference(out, tree->getlval());
  out << ",\"root\":";
  expressionReference(out, tree->getRoot());
  out << '}';
}

const char* operationName(OPType operation) {
  static constexpr const char* names[] = {
    "EMPTY", "MUX", "ADD", "SUB", "MUL", "DIV", "REM", "LT", "LEQ", "GT", "GEQ", "EQ", "NEQ",
    "DSHL", "DSHR", "AND", "OR", "XOR", "CAT", "ASUINT", "ASSINT", "ASCLOCK", "ASASYNCRESET",
    "CVT", "NEG", "NOT", "ANDR", "ORR", "XORR", "PAD", "SHL", "SHR", "HEAD", "TAIL", "BITS",
    "BITS_NOSHIFT", "INDEX_INT", "INDEX", "WHEN", "PRINTF", "ASSERT", "EXIT", "INT", "GROUP",
    "READ_MEM", "WRITE_MEM", "INFER_MEM", "INVALID", "RESET", "SEXT", "EXT_FUNC", "STMT_SEQ",
    "STMT_WHEN", "STMT_NODE"
  };
  static_assert(sizeof(names) / sizeof(names[0]) == OP_STMT_NODE + 1);
  const size_t index = static_cast<size_t>(operation);
  if (index >= sizeof(names) / sizeof(names[0])) throw std::runtime_error("Unknown expression operation");
  return names[index];
}

// Exact active expression DAG. Native ENode IDs preserve sharing between
// assignments, reset trees and distinct physical owners. No allocator addresses
// or code-generation caches enter this representation.
void structure(std::ostream& out, const Node* node) {
  out << "{\"algorithm\":\"native-expression-dag-v1\",\"fingerprint\":\""
      << StructureFingerprint().node(node) << "\",\"assignTreeCount\":" << node->assignTree.size()
      << ",\"assignTrees\":[";
  std::vector<const ENode*> pending;
  const auto roots = [&](const ExpTree* tree) {
    if (tree) {
      pending.push_back(tree->getlval());
      pending.push_back(tree->getRoot());
    }
  };
  for (size_t i = 0; i < node->assignTree.size(); ++i) {
    if (i) out << ',';
    expressionTree(out, node->assignTree[i]);
    roots(node->assignTree[i]);
  }
  out << "],\"resetTree\":";
  expressionTree(out, node->resetTree);
  roots(node->resetTree);
  std::unordered_set<const ENode*> seen;
  std::vector<const ENode*> expressions;
  while (!pending.empty()) {
    const ENode* expression = pending.back();
    pending.pop_back();
    if (!expression || !seen.insert(expression).second) continue;
    expressions.push_back(expression);
    pending.insert(pending.end(), expression->child.begin(), expression->child.end());
  }
  std::sort(expressions.begin(), expressions.end(), [](const ENode* a, const ENode* b) {
    return a->id < b->id;
  });
  out << ",\"expressions\":[";
  for (size_t i = 0; i < expressions.size(); ++i) {
    const ENode* expression = expressions[i];
    if (i) out << ',';
    out << "{\"id\":" << expression->id << ",\"op\":" << static_cast<int>(expression->opType)
        << ",\"opName\":\"" << operationName(expression->opType) << '"'
        << ",\"width\":" << expression->width << ",\"signed\":" << (expression->sign ? "true" : "false")
        << ",\"isClock\":" << (expression->isClock ? "true" : "false")
        << ",\"reset\":" << static_cast<int>(expression->reset)
        << ",\"usedBit\":" << expression->usedBit << ",\"node\":";
    nullableReference(out, expression->nodePtr);
    out << ",\"memory\":";
    nullableReference(out, expression->memoryNode);
    out << ",\"value\":\"" << escape(expression->strVal) << "\",\"integers\":";
    integers(out, expression->values);
    out << ",\"children\":[";
    for (size_t child = 0; child < expression->child.size(); ++child) {
      if (child) out << ',';
      expressionReference(out, expression->child[child]);
    }
    out << "]}";
  }
  out << "],\"clock\":";
  nullableReference(out, node->clock);
  out << ",\"regNext\":";
  nullableReference(out, node->regNext);
  out << ",\"parent\":";
  nullableReference(out, node->parent);
  out << ",\"isClock\":" << (node->isClock ? "true" : "false")
      << ",\"reset\":" << static_cast<int>(node->reset)
      << ",\"asReset\":" << static_cast<int>(node->asReset)
      << ",\"regSplit\":" << (node->regSplit ? "true" : "false")
      << ",\"inAggregate\":" << (node->inAggr ? "true" : "false")
      << ",\"usedBit\":" << node->usedBit << ",\"status\":" << static_cast<int>(node->status)
      << ",\"members\":[";
  for (size_t i = 0; i < node->member.size(); ++i) {
    if (i) out << ',';
    nullableReference(out, node->member[i]);
  }
  out << "],\"externalPorts\":[";
  for (size_t i = 0; i < node->externalPorts.size(); ++i) {
    if (i) out << ',';
    nullableReference(out, node->externalPorts[i]);
  }
  out << "],\"params\":[";
  for (size_t i = 0; i < node->params.size(); ++i) {
    if (i) out << ',';
    out << "{\"isInt\":" << (node->params[i].first ? "true" : "false")
        << ",\"value\":\"" << escape(node->params[i].second) << "\"}";
  }
  out << "],\"extraInfo\":\"" << escape(node->extraInfo) << '"';
  if (node->type == NODE_MEMORY)
    out << ",\"memory\":{\"depth\":" << node->depth << ",\"readLatency\":" << node->rlatency
        << ",\"writeLatency\":" << node->wlatency << '}';
  out << '}';
}

Fields entityFields(const Node* node) {
  Fields fields;
  fields.emplace_back("name", '"' + escape(node->name) + '"');
  fields.emplace_back("opKind", std::to_string(static_cast<int>(node->type)));
  std::ostringstream physical;
  physical << "{\"width\":" << node->width << ",\"signed\":" << (node->sign ? "true" : "false")
           << ",\"shape\":";
  integers(physical, node->dimension);
  physical << '}';
  fields.emplace_back("physical", physical.str());
  std::ostringstream sources;
  sources << "[{\"path\":[";
  for (size_t i = 0; i < node->semanticPath.size(); ++i) {
    if (i) sources << ',';
    sources << '"' << escape(node->semanticPath[i]) << '"';
  }
  sources << "],\"scopeDepth\":" << node->semanticScopeDepth
          << ",\"line\":" << (node->sourceLine >= 0 ? node->sourceLine : node->lineno)
          << ",\"role\":\"" << sourceRole(node) << "\",\"shape\":";
  integers(sources, node->semanticShape);
  sources << ",\"shapeFieldDepths\":";
  integers(sources, node->semanticShapeFieldDepths);
  sources << ",\"projection\":";
  integers(sources, node->semanticIndices);
  sources << ",\"width\":" << node->width << ",\"signed\":" << (node->sign ? "true" : "false");
  if (!node->sourceOriginRole.empty()) sources << ",\"originRole\":\"" << escape(node->sourceOriginRole) << '"';
  if (node->sourceMemoryDepth >= 0) sources << ",\"storageDepth\":" << node->sourceMemoryDepth;
  sources << "}]";
  fields.emplace_back("sources", sources.str());
  std::ostringstream expression;
  structure(expression, node);
  fields.emplace_back("structure", expression.str());
  std::ostringstream sourceMetadata;
  sourceMetadata << "{\"kind\":" << static_cast<int>(node->sourceKind)
                 << ",\"line\":" << node->sourceLine << ",\"ownerId\":" << node->sourceOwnerId
                 << ",\"effectKind\":\"" << escape(node->sourceEffectKind) << "\",\"axes\":";
  integers(sourceMetadata, node->semanticAxes);
  sourceMetadata << ",\"lineno\":" << node->lineno << '}';
  fields.emplace_back("sourceMetadata", sourceMetadata.str());
  if ((node->type == NODE_REG_SRC || node->type == NODE_REG_DST) && node->regNext) {
    std::ostringstream pair;
    reference(pair, node->regNext);
    fields.emplace_back("registerPair", pair.str());
  }
  return fields;
}

void fieldObject(std::ostream& out, const Fields& fields) {
  out << '{';
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i) out << ',';
    out << '"' << fields[i].first << "\":" << fields[i].second;
  }
  out << '}';
}

void spoolFields(std::ostream& out, const Fields& fields) {
  out << fields.size() << '\n';
  for (const auto& field : fields) out << field.first << '\n' << field.second << '\n';
}

Fields readFields(std::istream& in, uint64_t offset) {
  in.clear();
  in.seekg(static_cast<std::streamoff>(offset));
  std::string line;
  if (!std::getline(in, line)) throw std::runtime_error("Cannot read native replay before body");
  const size_t count = std::stoull(line);
  Fields fields;
  for (size_t i = 0; i < count; ++i) {
    std::string name, value;
    if (!std::getline(in, name) || !std::getline(in, value))
      throw std::runtime_error("Incomplete native replay before body");
    fields.emplace_back(std::move(name), std::move(value));
  }
  return fields;
}

const std::string* fieldValue(const Fields& fields, const std::string& name) {
  for (const auto& field : fields) if (field.first == name) return &field.second;
  return nullptr;
}

void mutation(std::ostream& out, int id, const char* action, const Fields& set,
              const std::vector<std::string>& unset, const Fields& expect) {
  out << id << '\n' << "\"action\":\"" << action << "\",\"ref\":";
  reference(out, id);
  out << ",\"set\":";
  fieldObject(out, set);
  out << ",\"unset\":[";
  for (size_t i = 0; i < unset.size(); ++i) {
    if (i) out << ',';
    out << '"' << unset[i] << '"';
  }
  out << "],\"expect\":";
  fieldObject(out, expect);
  out << '\n';
}

} // namespace

NodeEvolutionTrace::NodeEvolutionTrace(const std::string& path,
    const std::string& graphName, const std::string& passName, const std::string& inputSha256) {
  const std::filesystem::path target(path);
  if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path());
  out.open(target);
  check();
  beforeSpoolPath = path + ".before-spool";
  mutationSpoolPath = path + ".mutation-spool";
  beforeSpool.open(beforeSpoolPath, std::ios::in | std::ios::out | std::ios::trunc);
  mutationSpool.open(mutationSpoolPath, std::ios::in | std::ios::out | std::ios::trunc);
  if (!beforeSpool || !mutationSpool) throw std::runtime_error("Cannot open native replay spool");
  out << "{\"record\":\"header\",\"schema\":\"wolvrix.node-evolution.v1\","
      << "\"engine\":\"gsim\",\"pass\":\"" << escape(passName)
      << "\",\"graph\":\"" << escape(graphName)
      << "\",\"scope\":\"live-node-and-memory\",\"inventoryScope\":\"all-live\",\"implicitIdentity\":true"
      << ",\"replayContract\":\"native-field-delta.v1\",\"reconstructionScope\":\"active-semantic-graph\""
      << ",\"excludedCaches\":[\"AST2Graph-only trees\",\"derived adjacency and scheduling\",\"code generation\"]";
  if (!inputSha256.empty())
    out << ",\"inputIdentity\":{\"sha256\":\"" << escape(inputSha256) << "\",\"authority\":\"caller\"}";
  out << "}\n";
  out.flush();
  check();
}

NodeEvolutionTrace::~NodeEvolutionTrace() {
  beforeSpool.close();
  mutationSpool.close();
  std::error_code error;
  std::filesystem::remove(beforeSpoolPath, error);
  std::filesystem::remove(mutationSpoolPath, error);
}

void NodeEvolutionTrace::check() {
  if (!out) {
    fprintf(stderr, "Cannot write native node evolution trace\n");
    exit(1);
  }
}

void NodeEvolutionTrace::requireState(State expected, const char* operation) {
  if (state != expected) {
    fprintf(stderr, "Invalid native node evolution lifecycle: %s\n", operation);
    exit(1);
  }
}

void NodeEvolutionTrace::inventory(const char* stage, const std::vector<Node*>& nodes) {
  if (!stage || (std::string(stage) != "before" && std::string(stage) != "after")) {
    fprintf(stderr, "Invalid native node evolution lifecycle: unknown inventory stage\n");
    exit(1);
  }
  const bool before = std::string(stage) == "before";
  requireState(before ? State::BeforeInventory : State::Events,
               before ? "before inventory requires a new trace" : "after inventory requires the before inventory");
  for (const Node* node : nodes) {
    if (boundary.size() <= static_cast<size_t>(node->id)) boundary.resize(node->id + 1);
    boundary[node->id] |= before ? 1 : 2;
    out << "{\"record\":\"entity\",\"stage\":\"" << stage << "\",\"ref\":";
    reference(out, node);
    const Fields fields = entityFields(node);
    for (const auto& field : fields) out << ",\"" << field.first << "\":" << field.second;
    out << "}\n";
    if (before) {
      if (beforeOffsets.size() <= static_cast<size_t>(node->id)) beforeOffsets.resize(node->id + 1);
      beforeOffsets[node->id] = static_cast<uint64_t>(beforeSpool.tellp()) + 1;
      spoolFields(beforeSpool, fields);
    } else if (static_cast<size_t>(node->id) >= beforeOffsets.size() || !beforeOffsets[node->id]) {
      mutation(mutationSpool, node->id, "create", fields, {}, {});
    } else {
      const Fields old = readFields(beforeSpool, beforeOffsets[node->id] - 1);
      Fields set, expect;
      std::vector<std::string> unset;
      for (const auto& field : fields) {
        const std::string* previous = fieldValue(old, field.first);
        if (previous && *previous == field.second) continue;
        set.push_back(field);
        if (previous) expect.emplace_back(field.first, *previous);
      }
      for (const auto& field : old) {
        if (fieldValue(fields, field.first)) continue;
        unset.push_back(field.first);
        expect.push_back(field);
      }
      if (!set.empty() || !unset.empty()) mutation(mutationSpool, node->id, "update", set, unset, expect);
    }
  }
  if (before) beforeCount += nodes.size();
  else {
    afterCount += nodes.size();
    for (size_t id = 0; id < boundary.size(); ++id) {
      if (boundary[id] != 1) continue;
      mutation(mutationSpool, id, "erase", {}, {}, readFields(beforeSpool, beforeOffsets[id] - 1));
    }
  }
  beforeSpool.flush();
  mutationSpool.flush();
  if (!beforeSpool || !mutationSpool) throw std::runtime_error("Cannot write native replay spool");
  out.flush();
  check();
  state = before ? State::Events : State::AfterInventory;
}

void NodeEvolutionTrace::event(const char* action, const char* reason,
    const std::vector<Node*>& before, const std::vector<Node*>& after,
    const std::string& geometry) {
  requireState(State::Events, "events require the before inventory and must precede the after inventory");
  Step step{action, reason, {}, {}, geometry};
  for (const Node* node : before) step.before.push_back(node->id);
  for (const Node* node : after) step.after.push_back(node->id);
  steps.push_back(std::move(step));
}

void NodeEvolutionTrace::finish() {
  requireState(State::AfterInventory, "finish requires both inventories and can only be called once");
  std::vector<size_t> parent(steps.size());
  std::iota(parent.begin(), parent.end(), 0);
  std::vector<size_t> firstStep(boundary.size(), 0);
  const auto find = [&](size_t index) {
    size_t root = index;
    while (parent[root] != root) root = parent[root];
    while (parent[index] != index) {
      const size_t next = parent[index];
      parent[index] = root;
      index = next;
    }
    return root;
  };
  for (size_t i = 0; i < steps.size(); ++i) {
    for (const auto* refs : {&steps[i].before, &steps[i].after}) {
      for (int id : *refs) {
        if (firstStep.size() <= static_cast<size_t>(id)) firstStep.resize(id + 1);
        if (firstStep[id]) {
          const size_t a = find(firstStep[id] - 1), b = find(i);
          parent[std::max(a, b)] = std::min(a, b);
        } else firstStep[id] = i + 1;
      }
    }
  }
  std::vector<std::vector<size_t>> components(steps.size());
  std::vector<size_t> boundaryEvent(boundary.size(), 0);
  std::vector<bool> mutableEvents(1, false);
  for (size_t i = 0; i < steps.size(); ++i) components[find(i)].push_back(i);
  for (const auto& component : components) {
    if (component.empty()) continue;
    std::vector<int> before, after;
    bool onlyRetain = true;
    bool pureSplit = component.size() == 1 && steps[component.front()].action == "split";
    for (size_t i : component) {
      const Step& step = steps[i];
      onlyRetain &= step.action == "retain";
      for (const auto* refs : {&step.before, &step.after}) {
        for (int id : *refs) {
          if (static_cast<size_t>(id) >= boundary.size()) continue;
          if (boundary[id] & 1) before.push_back(id);
          if (boundary[id] & 2) after.push_back(id);
        }
      }
    }
    for (auto* refs : {&before, &after}) {
      std::sort(refs->begin(), refs->end());
      refs->erase(std::unique(refs->begin(), refs->end()), refs->end());
    }
    // A component consisting exclusively of temporary/rolled-back entities
    // has no boundary effect. Keep all internal steps for surviving components.
    if (before.empty() && after.empty()) continue;
    const char* action = onlyRetain ? "retain" : pureSplit ? "split" :
                         before.empty() ? "create" : after.empty() ? "delete" : "rewrite";
    mutableEvents.push_back(!onlyRetain);
    out << "{\"record\":\"event\",\"seq\":" << ++eventCount
        << ",\"action\":\"" << action << "\",\"reason\":\"" << escape(steps[component.front()].reason)
        << "\",\"before\":";
    references(out, before);
    out << ",\"after\":";
    references(out, after);
    out << ",\"geometry\":{\"partition\":" << (pureSplit ? "true" : "false") << "},\"steps\":[";
    for (size_t position = 0; position < component.size(); ++position) {
      const size_t i = component[position];
      const Step& step = steps[i];
      if (position) out << ',';
      out << "{\"seq\":" << i + 1 << ",\"action\":\"" << escape(step.action)
          << "\",\"reason\":\"" << escape(step.reason) << "\",\"before\":";
      references(out, step.before);
      out << ",\"after\":";
      references(out, step.after);
      if (!step.geometry.empty()) out << ",\"geometry\":" << step.geometry;
      out << '}';
    }
    out << "]}\n";
    for (const auto* refs : {&before, &after})
      for (int id : *refs) boundaryEvent[id] = eventCount;
    check();
  }
  mutationSpool.seekg(0);
  std::string idLine, body;
  while (std::getline(mutationSpool, idLine)) {
    if (!std::getline(mutationSpool, body)) throw std::runtime_error("Incomplete native replay mutation");
    const size_t id = std::stoull(idLine);
    if (id >= boundaryEvent.size() || !boundaryEvent[id]) {
      out.flush();
      throw std::runtime_error("Native replay mutation has no recorded evolution event for node " + idLine);
    }
    if (!mutableEvents[boundaryEvent[id]]) {
      out.flush();
      throw std::runtime_error("Native replay mutation is attributed only to a retain event for node " + idLine);
    }
    out << "{\"record\":\"mutation\",\"seq\":" << ++mutationCount
        << ",\"eventSeq\":" << boundaryEvent[id] << ',' << body << "}\n";
  }
  out << "{\"record\":\"footer\",\"complete\":true,\"beforeCount\":" << beforeCount
      << ",\"afterCount\":" << afterCount << ",\"eventCount\":" << eventCount
      << ",\"mutationCount\":" << mutationCount << ",\"replayComplete\":true}\n";
  out.flush();
  check();
  state = State::Finished;
}
