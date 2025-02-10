#include "common.h"

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

void graph::dump(std::string FileName) {
  extern std::string OutputDir;
  std::ofstream ofs{OutputDir + "/" + this->name + "_" + FileName + ".dot"};
  GraphDumper(ofs).dump(this);
}
