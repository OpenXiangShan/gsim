#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";

const char* cmpOP[][2] = {{"s_mpz_lt", "<"}, {"s_mpz_leq", "<="}, {"s_mpz_gt", ">"}, {"s_mpz_geq", ">="}, {"s_mpz_eq", "=="}, {"s_mpz_neq", "!="}};

#define UI(x) (std::string("mpz_get_ui(") + x + ")")
#define waitIdx(x) (x + "_waitIdx")
#define timer(x) (x + "_timer")
#define valid(x) (x + "_valid")
void topoSort(graph* g);

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  INCLUDE_LIB(hfile, "iostream");
  INCLUDE_LIB(hfile, "vector");
  INCLUDE_LIB(hfile, "gmp.h");
  INCLUDE_LIB(hfile, "assert.h");
  INCLUDE(hfile, "functions.h");
  hfile << "class S" << g->name << "{\n" << "public:\n";
/*
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
*/
// constructor
  hfile << "S" << g->name << "() {" << std::endl;
  for(Node* node: g->sorted) hfile << "mpz_init2(" << node->name << ", " << node->width << ");\n";
  hfile << "mpz_init(oldVal);\n";
  hfile << "}\n";

// active flags
  hfile << "std::vector<bool> activeFlags = " << "std::vector<bool>(" <<g->sorted.size() << ", true);\n";
// all sigs
  for (Node* node: g->sorted) {
    hfile << "mpz_t " << node->name << ";\n";
  }
// unique oldVal
  hfile << "mpz_t oldVal;\n";
// tmp variable
  for (int i = 0; i < g->maxTmp; i++) hfile << "mpz_t tmp" << i << ";\n";
// memory
  for(Node* n : g->memory) {
    for(Node* rw : n->member) {
      for(Node* type : rw->member) {
        hfile << "mpz_t " << type->name << ";\n";
      }
      if(rw->type == NODE_READER) {
        if(n->latency[0] != 0){
          hfile << "int " << waitIdx(rw->name) << ";\n";
          hfile << "unsigned long " << timer(rw->name) << "[" << n->latency[0] + 1 << "];\n";
        }
      } else if(rw->type == NODE_WRITER) {
        if(n->latency[1] != 0){
          hfile << "int " << waitIdx(rw->name) << ";\n";
          hfile << "unsigned long " << timer(rw->name) << "[" << n->latency[1] + 1 << "][2];\n";
          hfile << "bool " << timer(rw->name) << "[" << n->latency[1] + 1 << "];\n";
        }
      }
    }
  }

// set functions
  for (Node* node: g->input) {
    hfile << "void set_" << node->name << "(mpz_t val) {\n";
    hfile <<"mpz_set(" << node->name << ", val);\n";
    for (Node* next: node->next)
      hfile << "activeFlags[" << next->id << "] = true;\n";
    hfile << "}\n";
  }
// step functions
  for (int i = 0; i < g->sorted.size(); i++) {
    hfile << "void step" << i << "();\n";
  }

// functions
  hfile << "void step();\n";
  hfile << "};\n";
  hfile << "#endif\n";
  hfile.close();
}

void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  INCLUDE(sfile, headerFile + ".h");

  for(Node* node: g->sorted) {
    if(node->insts.size() == 0) continue;
    // generate function
    sfile << "void S" << g->name << "::step" << node->id << "() {\n";
    if(node->type == NODE_REG_SRC || node->type == NODE_REG_DST || node->type == NODE_OTHERS) {
      sfile << "activeFlags[" << node->id << "] = false;\n";
      sfile << "mpz_set(oldVal, " << node->name << ");\n";
      for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
      Node* activeNode = node->type == NODE_REG_DST ? node->regNext : node;
      if(activeNode->next.size() > 0){
        sfile << "if(" << "mpz_cmp(oldVal," << node->name << ") != 0){\n";
        for(Node* next: activeNode->next) {
          sfile << "activeFlags[" << next->id << "] = true;\n";
        }
        sfile << "}\n";
      }
      sfile << "std::cout << \"" << node->id  << ": " << node->name << "(" << node->width << "): \" ;";
      sfile << "mpz_out_str(stdout, 16, oldVal); std::cout << \" -> \"; mpz_out_str(stdout, 16, " << node->name << "); std::cout << std::endl;\n";
    } else if(node->type == NODE_READER) {
      int readlatency = node->regNext->latency[0];
      if(readlatency == 0) {
        sfile << "activeFlags[" << node->id << "] = false;\n";
        sfile << UI(node->member[3]->name) << " = " << node->regNext->name << "[" << UI(node->member[0]->name) << "]\n";
      } else {
        for(int i = 0; i < readlatency; i++)
          sfile << timer(node->name) << "[" << i << "] = " << timer(node->name) << "[" << i + 1 << "];\n";
        sfile << timer(node->name) << "[" << readlatency << "] = " << UI(node->member[0]->name) << ";\n";
        sfile << "if(" << waitIdx(node->name) << " == 0) activeFlags[" << node->id << "] = false;\n";
        sfile << "if(" << UI(node->member[1]->name) << ") " << waitIdx(node->name) << " = " << readlatency << ";\n";
        sfile << "else " << waitIdx(node->name) << " = " << waitIdx(node->name) << " == 0 ? 0 : " << waitIdx(node->name) << " - 1;\n";
        sfile << UI(node->member[3]->name) << " = " << node->regNext->name << "[" << timer(node->name) << "[0]]\n";
      }
    } else if(node->type == NODE_WRITER){
      int writelatency = node->regNext->latency[1];
      if(writelatency == 0) {
        sfile << "activeFlags[" << node->id << "] = false;\n";
        sfile << "if(" << UI(node->member[1]->name) << "&&" << UI(node->member[4]->name) << ") " << node->regNext->name << "[" << UI(node->member[0]->name) << "] = " << node->member[3] << ";\n";
      } else {
        for(int i = 0; i < writelatency; i++){
          sfile << timer(node->name) << "[" << i << "][0] = " << timer(node->name) << "[" << i + 1 << "][0];\n";
          sfile << timer(node->name) << "[" << i << "][1] = " << timer(node->name) << "[" << i + 1 << "][1];\n";
          sfile << valid(node->name) << "[" << i << "] = " << valid(node->name) << "[" << i + 1 << "];\n";
        }
        sfile << timer(node->name) << "[" << writelatency << "][0] = " << UI(node->member[0]->name) << ";\n";
        sfile << timer(node->name) << "[" << writelatency << "][1] = " << UI(node->member[3]->name) << ";\n";
        sfile << valid(node->name) << "[" << writelatency << "] = " << UI(node->member[1]->name) << "&&" << UI(node->member[4]->name) <<  ";\n";
        sfile << "if(" << waitIdx(node->name) << " == 0) activeFlags[" << node->id << "] = false;\n";
        sfile << "if(" << UI(node->member[1]->name) << "&&" << UI(node->member[4]->name) << ") " << waitIdx(node->name) << " = " << writelatency << ";\n";
        sfile << "else " << waitIdx(node->name) << " = " << waitIdx(node->name) << " == 0 ? 0 : " << node->name << "_waitIdx - 1;\n";
        sfile << node->regNext->name << "[" << timer(node->name) << "[0][0]] = " << timer(node->name) << "[0][1]]";
      }
    } else {
      Assert(0, "Invalid node %s with type %d\n", node->name.c_str(), node->type);
    }
    sfile << "}\n";

  }

  sfile << "" <<"void S" << g->name << "::step() {\n";
  for(int i = 0; i < g->sorted.size(); i++) {
    if(g->sorted[i]->insts.size() == 0) continue;
    sfile << "if(activeFlags[" << i << "]) " << "step" << i << "();\n";
  }

  // active nodes
  for(Node* n: g->active) {
    for(int i = 0; i < n->insts.size(); i ++) sfile << n->insts[i] << ";\n";
  }

  // update registers
  for(Node* node: g->sources) {
    sfile << "mpz_set(" << node->name << ", " << node->name << "_next);\n";
  }
  // sfile << "std::cout << \"------\\n\";\n";
  sfile << "}";
}

void generator(graph* g, std::string header, std::string src) {
  topoSort(g);
  genHeader(g, header);
  genSrc(g, header, src);
}
