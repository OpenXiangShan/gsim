#include "common.h"
#include "Node.h"
#include "graph.h"

void topoSort(graph* g);

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  hfile << "class S" << g->name << "{\n" << "public:\n";
  // ports
  for(Node* node: g->input) {
    hfile << "int " << node->name << ";\n";
  }
  for(Node* node: g->output) {
    hfile << "int " << node->name << ";\n";
  }
  // variables
  for(Node* node: g->sources) {
    hfile << "int " << node->name << ";\n";
  }
  // functions
  hfile << "void step();\n";
  hfile << "}\n";
  hfile << "#endif\n";
  hfile.close();
}

void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  sfile << "#include " << "\"" << headerFile << ".h\"\n" << "" <<"void S" << g->name << "::step() {\n";
  for(Node* node: g->sorted) {
    if(node->op.length() == 0) continue;
    if(!node->defined) sfile << "int ";
    sfile << node->name << " = " << node->op << ";\n";
  }
  // update registers
  for(Node* node: g->sources) {
    sfile << node->name << " = " << node->name << "_next;\n";
  }
  sfile << "}";
}

void generator(graph* g, std::string header, std::string src) {
  genHeader(g, header);
  topoSort(g);
  genSrc(g, header, src);
}
