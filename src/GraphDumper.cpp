#include "common.h"

class GraphDumper {
 public:
  GraphDumper(std::ostream& os) : os(os) {}

  void dump(const graph* g);

 private:
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

  for (auto* N : g->sortedSuper) dump(N);

  for (auto* Super : g->sortedSuper)
    for (auto* Member : Super->member)
      for (auto* Next : Member->next) os << "\t\t\"" << Member->name << "\" -> \"" << Next->name << "\";\n";

  os << "}\n";
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
  std::ofstream ofs{std::string{OBJ_DIR} + "/" + this->name + "_" + FileName + ".dot"};
  GraphDumper(ofs).dump(this);
}