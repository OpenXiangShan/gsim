#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";
#define T_NUM 1

#define UI(x) (std::string("mpz_get_ui(") + x + ")")
void topoSort(graph* g);
#define STEP_START(file, g, node) file << "void S" << g->name << "::step" << node->id << "() {\n"
#define SET_OLDVAL(file, node) file << "mpz_set(oldVal, " << node->name << ");\n"
#define ACTIVATE(file, node, nextNodes) do { \
    if(nextNodes.size() > 0){ \
      file << "if(" << "mpz_cmp(oldVal," << node->name << ") != 0){\n"; \
      for(Node* next: nextNodes) { \
        file << "activeFlags[" << next->id << "] = true;\n"; \
      } \
      file << "}\n"; \
    } \
  } while(0)

#if 1
#define EMU_LOG(file, id, node) do{ \
        file << "std::cout << \"" << id  << ": " << node->name << "(" << node->width << ", " << node->sign << "): \" ;"; \
        file << "mpz_out_str(stdout, 16, oldVal); std::cout << \" -> \"; mpz_out_str(stdout, 16, " << node->name << "); std::cout << std::endl;\n"; \
      } while(0)
#define WRITE_LOG(file, name, idx, val) do{ \
        file << "std::cout << \"" << name << "[\";"; \
        file << "mpz_out_str(stdout, 10, " << idx << ");"; \
        file << "std::cout << \"] = \"; mpz_out_str(stdout, 16, " << val << "); std::cout << std::endl;\n"; \
        } while(0)
#else
#define EMU_LOG(...) 
#define WRITE_LOG(...) 
#endif

void genHeader(graph* g, std::string headerFile) {
  std::ofstream hfile(std::string(OBJ_DIR) + "/" + headerFile + ".h");
#ifdef DIFFTEST_PER_SIG
  std::ofstream sigFile(std::string(OBJ_DIR) + "/" + "allSig.h");
#endif

  hfile << "#ifndef " << headerFile << "_H\n";
  hfile << "#define " << headerFile << "\n";
  INCLUDE_LIB(hfile, "iostream");
  INCLUDE_LIB(hfile, "vector");
  INCLUDE_LIB(hfile, "gmp.h");
  INCLUDE_LIB(hfile, "assert.h");
  INCLUDE(hfile, "functions.h");
  hfile << "class S" << g->name << "{\n" << "public:\n";

// constructor
  hfile << "S" << g->name << "() {" << std::endl;
  hfile << "void init_functions();\n";
  for(int i = 0; i < T_NUM; i++) {
    hfile << "mpz_init(t" << i << ");\n";
  }
  for(Node* node: g->sorted) {
    switch(node->type) {
      case NODE_READER: case NODE_WRITER:
        for(Node* member : node->member) {
          hfile << "mpz_init2(" << member->name << ", " << member->width << ");\n";
        }
        break;
      case NODE_REG_SRC:
#ifdef DIFFTEST_PER_SIG
        hfile << "mpz_init2(" << node->name << "$prev" << ", " << node->width << ");\n";
#endif
      default:
        hfile << "mpz_init2(" << node->name << ", " << node->width << ");\n";
    }
  }
  hfile << "mpz_init(oldVal);\n";
  hfile << "}\n";

// active flags
  hfile << "std::vector<bool> activeFlags = " << "std::vector<bool>(" <<g->sorted.size() << ", true);\n";
// all sigs
  for (Node* node: g->sorted) {
    switch(node->type) {
      case NODE_READER: case NODE_WRITER:
        for(Node* member : node->member) {
          hfile << "mpz_t " << member->name << ";\n";
        }
        break;
      case NODE_L1_RDATA:
        break;
      case NODE_REG_SRC:
#ifdef DIFFTEST_PER_SIG
        hfile << "mpz_t " << node->name << "$prev;\n";
#endif
      default:
        hfile << "mpz_t " << node->name << ";\n";
#ifdef DIFFTEST_PER_SIG
        std::string name = "newtop__DOT__" +node->name;
        int pos;
        while((pos = name.find("$")) != std::string::npos) {
          if(name.substr(pos + 1, 2) == "io") {
            name.replace(pos, 1, "_");
          }
          else name.replace(pos, 1, "__DOT__");
        }
        if(node->type == NODE_REG_SRC)
          sigFile << node->name + "$prev" << " " << name << std::endl;
        else
          sigFile << node->name << " " << name << std::endl;
#endif
    }
  }
// unique oldVal
  hfile << "mpz_t oldVal;\n";
// tmp variable
  for (int i = 0; i < T_NUM; i++) hfile << "mpz_t t" << i << ";\n";
  for (int i = 0; i < g->maxTmp; i++) hfile << "mpz_t tmp" << i << ";\n";
// memory
  for(Node* n : g->memory) {
    hfile <<  (n->width == 8 ? "uint8_t " : 
              (n->width == 16 ? "uint16_t " : 
              (n->width == 32 ? "uint32_t " : 
              (n->width == 64 ? "uint64_t " : "struct{uint64_t hi; uint64_t lo;}"))));
    Assert(n->width <= 128, "Data type of memory %s is larger than 128\n", n->name.c_str());
    hfile << n->name << "[" << n->val << "];\n";
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
    // generate function
    Node* activeNode;
    int latency;
    switch(node->type) {
      case NODE_REG_SRC: case NODE_REG_DST: case NODE_OTHERS: case NODE_OUT: case NODE_L1_RDATA:
        if(node->insts.size() == 0) continue;
        // sfile << "void S" << g->name << "::step" << node->id << "() {\n";
        STEP_START(sfile, g, node);
        sfile << "activeFlags[" << node->id << "] = false;\n";
        SET_OLDVAL(sfile, node);
        // sfile << "mpz_set(oldVal, " << node->name << ");\n";
        for(int i = 0; i < node->insts.size(); i ++) sfile << node->insts[i] << ";\n";
        activeNode = node->type == NODE_REG_DST ? node->regNext : node;
        ACTIVATE(sfile, activeNode, activeNode->next);
        EMU_LOG(sfile, node->id, node);
        break;
      case NODE_READER:
        STEP_START(sfile, g, node);
        latency = node->regNext->latency[0];
        if(latency == 0) SET_OLDVAL(sfile, node->member[3]);
        for(Node* member: node->member) {
          for(int i = 0; i < member->insts.size(); i ++) sfile << member->insts[i] << ";\n";
        }
        if(latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          if(node->regNext->width == 128) {
            sfile << "mpz_set_ui(" << node->member[3]->name << ", " << node->regNext->name << "[" << UI(node->member[0]->name) << "].hi);\n";
            sfile << "mpz_mul_2exp(" << node->member[3]->name << ", " << node->member[3]->name << ", " << 64 << ");\n";
            sfile << "mpz_add_ui(" << node->member[3]->name << ", " << node->member[3]->name << ", " << node->regNext->name << "[" << UI(node->member[0]->name) << "].lo);\n";
          } else {
            sfile << "mpz_set_ui(" << node->member[3]->name << ", " << node->regNext->name << "[" << UI(node->member[0]->name) << "]);\n";
          }
          ACTIVATE(sfile, node->member[3], node->next);
          EMU_LOG(sfile, node->id, node->member[3]);
        }
        break;
      case NODE_WRITER:
        STEP_START(sfile, g, node);
        for(Node* member: node->member) {
          for(int i = 0; i < member->insts.size(); i ++) sfile << member->insts[i] << ";\n";
        }
        latency = node->regNext->latency[1];
        if(latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          sfile << "if(" << UI(node->member[1]->name) << "&&" << UI(node->member[4]->name) << ") {\n";
          if(node->regNext->width == 128) {
            sfile << "mpz_div_2exp(t0, " << node->member[3]->name << ", 64);\n";
            sfile << node->regNext->name << "[" << UI(node->member[0]->name) << "].hi = " << UI("t1") << ";\n";
            sfile << node->regNext->name << "[" << UI(node->member[0]->name) << "].lo = " << UI(node->member[3]->name) << ";\n";
          } else {
            sfile << node->regNext->name << "[" << UI(node->member[0]->name) << "] = " << UI(node->member[3]->name) << ";\n";
          }
          WRITE_LOG(sfile, node->regNext->name, node->member[0]->name, node->member[4]->name);
          sfile << "}\n";
        }
        break;
      case NODE_INP:
        continue;
      default:
        Assert(0, "Invalid node %s with type %d\n", node->name.c_str(), node->type);
    }
    sfile << "}\n";

  }

  sfile << "" <<"void S" << g->name << "::step() {\n";
  for(int i = 0; i < g->sorted.size(); i++) {
    if(g->sorted[i]->insts.size() == 0 && g->sorted[i]->type != NODE_READER && g->sorted[i]->type != NODE_WRITER) continue;
    sfile << "if(activeFlags[" << i << "]) " << "step" << i << "();\n";
  }

  // active nodes
  for(Node* n: g->active) {
    for(int i = 0; i < n->insts.size(); i ++) sfile << n->insts[i] << ";\n";
  }

  // update registers
  for(Node* node: g->sources) {
    if(node->next.size()) {
      sfile << "if(mpz_cmp(" << node->name << ", " << node->name << "$next)) {\n";
      for(Node* next : node->next) sfile << "activeFlags[" << next->id << "] = true;\n";
      sfile << "}\n";
    }
#ifdef DIFFTEST_PER_SIG
    sfile << "mpz_set(" << node->name << "$prev, " << node->name << ");\n";
#endif
    sfile << "mpz_set(" << node->name << ", " << node->name << "$next);\n";
  }
  // update memory rdata & wdata
  for(Node* node: g->memory) {
    for(Node* rw: node->member) {
      if(rw->type == NODE_READER && node->latency[0] == 1) {
        Node* data = rw->member[3];
        Assert(data->type == NODE_L1_RDATA, "Invalid data type(%d) for reader(%s) with latency 1\n", data->type, rw->name.c_str());
        sfile << "mpz_set(oldVal, " << data->name << ");\n";
        sfile << "mpz_set_ui(" << data->name << ", " << node->name << "[" << UI(rw->member[0]->name) << "]);\n";
        if(data->next.size() > 0) {
          sfile << "if(" << "mpz_cmp(oldVal," << data->name << ") != 0){\n";
          for(Node* next : data->next) {
            sfile << "activeFlags[" << next->id << "] = true;\n";
          }
          sfile << "}\n";
        }
      } else if(rw->type == NODE_WRITER && node->latency[1] == 1) {
        sfile << "if(" << UI(rw->member[1]->name) << " && " << UI(rw->member[4]->name) << ") {\n";
        if(node->width == 128) {
          sfile << "mpz_div_2exp(t0, " << rw->member[3]->name << ", 64);\n";
          sfile << node->name << "[" << UI(rw->member[0]->name) << "].hi = " << UI("t0") << ";\n";
          sfile << node->name << "[" << UI(rw->member[0]->name) << "].lo = " << UI(rw->member[3]->name) << ";\n";
        } else {
          sfile << node->name << "[" << UI(rw->member[0]->name) << "] = " << UI(rw->member[3]->name) << ";\n";
        }
        for(Node* reader: rw->regNext->member) {
          if(reader->type == NODE_READER) sfile << "activeFlags[" << reader->id << "] = true;\n";
        }
        WRITE_LOG(sfile, node->name, rw->member[0]->name, rw->member[3]->name);
        sfile << "}\n";
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
