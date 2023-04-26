#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";

const char* cmpOP[][2] = {{"s_mpz_lt", "<"}, {"s_mpz_leq", "<="}, {"s_mpz_gt", ">"}, {"s_mpz_geq", ">="}, {"s_mpz_eq", "=="}, {"s_mpz_neq", "!="}};

#define UI(x) (std::string("mpz_get_ui(") + x + ")")
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
    hfile << (n->width == 8 ? "uint8_t " : (n->width == 16 ? "uint16_t " : (n->width == 32 ? "uint32_t " : "uint64_t ")));
    hfile << n->name << "[" << n->val << "];\n";
    for(Node* rw : n->member) {
      for(Node* type : rw->member) {
        hfile << "mpz_t " << type->name << ";\n";
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
    Node* activeNode;
    int latency;
    sfile << "void S" << g->name << "::step" << node->id << "() {\n";
    switch(node->type) {
      case NODE_REG_SRC: case NODE_REG_DST: case NODE_OTHERS: case NODE_OUT:
        sfile << "activeFlags[" << node->id << "] = false;\n";
        sfile << "mpz_set(oldVal, " << node->name << ");\n";
        for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
        activeNode = node->type == NODE_REG_DST ? node->regNext : node;
        if(activeNode->next.size() > 0){
          sfile << "if(" << "mpz_cmp(oldVal," << node->name << ") != 0){\n";
          for(Node* next: activeNode->next) {
            sfile << "activeFlags[" << next->id << "] = true;\n";
          }
          sfile << "}\n";
        }
        // sfile << "std::cout << \"" << node->id  << ": " << node->name << "(" << node->width << "): \" ;";
        // sfile << "mpz_out_str(stdout, 16, oldVal); std::cout << \" -> \"; mpz_out_str(stdout, 16, " << node->name << "); std::cout << std::endl;\n";
        break;
      case NODE_READER:
        for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
        latency = node->regNext->latency[0];
        if(latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          sfile << UI(node->member[3]->name) << " = " << node->regNext->name << "[" << UI(node->member[0]->name) << "]\n";
        }
        break;
      case NODE_WRITER:
        for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
        latency = node->regNext->latency[1];
        if(latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          sfile << "if(" << UI(node->member[1]->name) << "&&" << UI(node->member[4]->name) << ") " << \
                node->regNext->name << "[" << UI(node->member[0]->name) << "] = " << UI(node->member[3]->name) << ";\n";
        }
        break;
      default:
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
  // update memory rdata & wdata
  for(Node* node: g->memory) {
    for(Node* rw: node->member) {
    std::cout << rw->name << " " << rw->type << " " << node->latency[0] << " " << node->latency[1] << "....\n";
      if(rw->type == NODE_READER && node->latency[0] == 1) {
        Node* data = rw->member[3];
        Assert(data->type == NODE_L1_RDATA, "Invalid data type(%d) for reader(%s) with latency 1\n", data->type, rw->name.c_str());
        sfile << "mpz_set(oldVal, " << data->name << ");\n";
        sfile << "mpz_set_ui(" << data->name << ", " << node->name << "[" << rw->member[0]->name << "]);\n";
        if(data->next.size() > 0) {
          sfile << "if(" << "mpz_cmp(oldVal," << data->name << ") != 0){\n";
          for(Node* next : data->next) {
            sfile << "activeFlags[" << next->id << "] = true;\n";
          }
          sfile << "}\n";
        }
      } else if(rw->type == NODE_WRITER && node->latency[1] == 1) {
        sfile << "if(" << UI(rw->member[1]->name) << " && " << UI(rw->member[4]->name) << ") " << \
                node->name << "[" << UI(rw->member[0]->name) << "] = " << UI(rw->member[3]->name) << ";\n";
      }
    }

  }
  // sfile << "std::cout << \"------\\n\";\n";
  sfile << "}";
}

void generator(graph* g, std::string header, std::string src) {
  topoSort(g);
  genHeader(g, header);
  genSrc(g, header, src);
}
