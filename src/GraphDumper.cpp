#include "common.h"
#include <set>
#include <map>
#include <vector>

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

class GraphJsonDumper {
 public:
  GraphJsonDumper(std::ostream& os) : os(os) {}
  void dump(const graph* g) {
    const auto& supers = g->sortedSuper.empty() ? g->supersrc : g->sortedSuper;
    std::set<std::pair<std::string, std::string>> edges;
    os << "{\n  \"nodes\": [\n";
    bool firstNode = true;
    for (const SuperNode* super : supers) {
      for (const Node* node : super->member) {
        if (!firstNode) os << ",\n";
        firstNode = false;
        os << "    {\"name\": \"" << node->name << "\", "
           << "\"type\": \"" << nodeTypeToStr(node->type) << "\", "
           << "\"super\": " << super->id << "}";
        for (const Node* next : node->next) edges.insert({node->name, next->name});
      }
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
}
