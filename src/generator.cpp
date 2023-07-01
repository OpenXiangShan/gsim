/**
 * @file generator.cpp
 * @brief generator
 */

#include "common.h"
#include "Node.h"
#include "graph.h"

#define INCLUDE_LIB(f, s) f << "#include <" << s << ">\n";
#define INCLUDE(f, s) f << "#include \"" << s << "\"\n";
#define OLDNAME(width) (width > 64 ? "oldVal" : "oldValBasic")

#define UI(x) (std::string("mpz_get_ui(") + x + ")")
#define nameUI(node) (node->width > 64 ? UI(node->name) : node->name)
#define nodeEqualsZero(node) (node->width > 64 ? "mpz_sgn(" + node->name + ")" : node->name)

#define STEP_START(file, g, node)                                    \
  do {                                                               \
    nodeNum++;                                                       \
    file << "void S" << g->name << "::step" << node->id << "() {\n"; \
  } while (0)
#define SET_OLDVAL(file, node) \
  file << (node->width > 64 ? "mpz_set(oldVal, " + node->name + ")" : "oldValBasic = " + node->name) << ";\n"
#define ACTIVATE(file, node, nextNodes)                                                                              \
  do {                                                                                                               \
    if (nextNodes.size() > 0) {                                                                                      \
      file << "if("                                                                                                  \
           << (node->width > 64 ? "mpz_cmp(oldVal, " + node->name + ") != 0" : "oldValBasic != " + node->name)       \
           << "){\n";                                                                                                \
      for (Node * next : nextNodes) {                                                                                \
        if (next->status == VALID_NODE && next->type != NODE_ACTIVE) {                                               \
          file << "activeFlags[" << (next->id == next->clusId ? next->id : next->clusNodes[0]->id) << "] = true;\n"; \
        }                                                                                                            \
      }                                                                                                              \
      file << "}\n";                                                                                                 \
    }                                                                                                                \
  } while (0)

#define MEM_WRITE(file, w, mem, idx, val)                                     \
  do {                                                                        \
    if (w == 128) {                                                           \
      file << "mpz_div_2exp(t0, " << val->name << ", 64);\n";                 \
      file << mem << "[" << nameUI(idx) << "].hi = " << UI("t0") << ";\n";    \
      file << mem << "[" << nameUI(idx) << "].lo = " << nameUI(val) << ";\n"; \
    } else {                                                                  \
      file << mem << "[" << nameUI(idx) << "] = " << nameUI(val) << ";\n";    \
    }                                                                         \
  } while (0)

#define MEM_READ(file, w, mem, addr, dst)                                                                          \
  do {                                                                                                             \
    if (w == 128) {                                                                                                \
      file << "mpz_set_ui(" << dst->name << ", " << mem << "[" << nameUI(addr) << "].hi);\n";                      \
      file << "mpz_mul_2exp(" << dst->name << ", " << dst->name << ", " << 64 << ");\n";                           \
      file << "mpz_add_ui(" << dst->name << ", " << dst->name << ", " << mem << "[" << nameUI(addr) << "].lo);\n"; \
    } else {                                                                                                       \
      if (dst->width > 64)                                                                                         \
        file << "mpz_set_ui(" << dst->name << ", " << mem << "[" << nameUI(addr) << "]);\n";                       \
      else                                                                                                         \
        file << dst->name << " = " << mem << "[" << nameUI(addr) << "];\n";                                        \
    }                                                                                                              \
  } while (0)

#define DISP_CLUS_INSTS(file, node)                                                                \
  do {                                                                                             \
    for (int clusIdx = node->clusNodes.size() - 1; clusIdx >= 0; clusIdx--) {                      \
      Node* clusMember = node->clusNodes[clusIdx];                                                 \
      for (size_t i = 0; i < clusMember->insts.size(); i++) file << clusMember->insts[i] << ";\n"; \
    }                                                                                              \
  } while (0)

#define DISP_INSTS(file, node)                                                       \
  do {                                                                               \
    for (size_t i = 0; i < node->insts.size(); i++) file << node->insts[i] << ";\n"; \
  } while (0)

#if 0
#define EMU_LOG(file, id, oldName, node)                                                                          \
  do {                                                                                                            \
    file << "if(cycles >= 0x13acf00) {\n";                                                                        \
    file << "std::cout << std::hex << \"" << id << ": " << node->name << "(" << node->width << ", " << node->sign \
         << "): \" ";                                                                                             \
    if (node->width > 64) {                                                                                       \
      file << ";mpz_out_str(stdout, 16," << oldName << "); std::cout << \" -> \"; mpz_out_str(stdout, 16, "       \
           << node->name << ");";                                                                                 \
    } else {                                                                                                      \
      file << " << +" << oldName << " << \" -> \" << +" << node->name << ";";                                     \
    }                                                                                                             \
    file << "std::cout << std::endl;\n}\n";                                                                       \
  } while (0)
#define WRITE_LOG(file, name, idx, val, width)            \
  do {                                                    \
    file << "if(cycles >= 0x13acf00) {\n";                \
    file << "std::cout << \"" << name << "[\";";          \
    file << "std::cout << +" << idx << ";";               \
    file << "std::cout << \"] = \"; ";                    \
    if (width > 64) {                                     \
      file << "mpz_out_str(stdout, 16, " << val << ");";  \
    } else {                                              \
      file << "std::cout << std::hex << +" << val << ";"; \
    }                                                     \
    file << "std::cout << std::endl;\n}\n";               \
  } while (0)
#define EMU_LOG2(file, id, node)                                                                                    \
  do {                                                                                                              \
    file << "if(cycles >= 0x13acf00) {\n";                                                                          \
    file << "std::cout << \"" << id << ": " << node->name << "(" << node->width << ", " << node->sign << "): \" ;"; \
    if (node->width > 64) {                                                                                         \
      file << "mpz_out_str(stdout, 16, " << node->name << "); ";                                                    \
    } else {                                                                                                        \
      file << "std::cout << std::hex << +" << node->name << ";";                                                    \
    }                                                                                                               \
    file << "std::cout << std::endl;\n}\n";                                                                         \
  } while (0)
#else
#define EMU_LOG(...)
#define WRITE_LOG(...)
#define EMU_LOG2(...)
#endif

static int nodeNum = 0;

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
  INCLUDE_LIB(hfile, "cstdint");
  INCLUDE(hfile, "functions.h");

  hfile << "#define likely(x) __builtin_expect(!!(x), 1)\n#define unlikely(x) "
           "__builtin_expect(!!(x), 0)\n";
  hfile << "class S" << g->name << "{\n"
        << "public:\n";

  // constructor
  hfile << "S" << g->name << "() {" << std::endl;
  hfile << "init_functions();\n";
  for (int i = 1; i <= g->maxTmp; i++) hfile << "mpz_init(__tmp__" << i << ");\n";
  hfile << "mpz_init(t0);\n";
  for (Node* node : g->sorted) {
    switch (node->type) {
      case NODE_READER:
      case NODE_WRITER:
        for (Node* member : node->member) {
          if (member->width > 64) hfile << "mpz_init2(" << member->name << ", " << member->width << ");\n";
          if (member->status == CONSTANT_NODE) {
            if (member->width > 64) {
              hfile << member->name << " = 0x" << member->consVal << ";\n";
            } else {
              if (member->width <= 64) {
                hfile << member->name << " = 0x" << member->consVal << ";\n";
              } else {
                if (member->consVal.length() <= 16)
                  hfile << "mpz_set_ui(" << member->name << ", 0x" << member->consVal << ");\n";
                else
                  hfile << "mpz_set_str(" << member->name << ", 16, \"" << member->consVal << "\");\n";
              }
            }
          }
        }
        break;
      case NODE_REG_SRC:
#ifdef DIFFTEST_PER_SIG
        if (node->width > 64) hfile << "mpz_init2(" << node->name << "$prev" << ", " << node->width << ");\n";
        if (!node->regSplit) break;
#endif
      default:
        if (node->width > 64) hfile << "mpz_init2(" << node->name << ", " << node->width << ");\n";
    }
  }
  hfile << "mpz_init(oldVal);\n";
  hfile << "oldValBasic = 0;\n";
  hfile << "}\n";

  // active flags
  hfile << "std::vector<bool> activeFlags = "
        << "std::vector<bool>(" << g->sorted.back()->id << ", true);\n";
  // all sigs
  std::string mpz_vals;
  for (Node* node : g->sorted) {
    switch (node->type) {
      case NODE_READER:
      case NODE_WRITER:
        for (Node* member : node->member) {
          if (member->width > 64)
            mpz_vals += "mpz_t " + member->name + ";\n";
          else
            hfile << nodeType(member) << " " << member->name << ";\n";
        }
        break;
      case NODE_L1_RDATA: break;
      case NODE_REG_SRC:
#ifdef DIFFTEST_PER_SIG
        if (node->width > 64)
          mpz_vals += "mpz_t " + node->name + "$prev;\n";
        else
          hfile << nodeType(node) << " " << node->name << "$prev" << ";\n";
#endif
        if (!node->regSplit) break;
      default:
        if (node->width > 64)
          mpz_vals += "mpz_t " + node->name + ";\n";
        else
          hfile << nodeType(node) << " " << node->name << ";\n";
#ifdef DIFFTEST_PER_SIG
        std::string name = "newtop__DOT__" + node->name;
        size_t pos;
        while ((pos = name.find("$")) != std::string::npos) {
          if (name.substr(pos + 1, 2) == "io") {
            name.replace(pos, 1, "_");
          } else
            name.replace(pos, 1, "__DOT__");
        }
        if (node->type == NODE_REG_DST)
          sigFile << node->sign << " " << node->width << " " << node->regNext->name + "$prev"
                  << " " << name << std::endl;
        else if (node->type != NODE_REG_SRC)
          sigFile << node->sign << " " << node->width << " " << node->name << " " << name << std::endl;
#endif
    }
  }
  hfile << mpz_vals;
  // unique oldVal
  hfile << "mpz_t oldVal;\n";
  hfile << "uint64_t oldValBasic;\n";
  // tmp variable
  hfile << "mpz_t t0;\n";
  for (int i = 1; i <= g->maxTmp; i++) hfile << "mpz_t __tmp__" << i << ";\n";
  // memory
  for (Node* n : g->memory) {
    hfile << (n->width == 8 ? "uint8_t " :
             (n->width == 16 ? "uint16_t " :
             (n->width == 32 ? "uint32_t " :
             (n->width == 64 ? "uint64_t " : "struct{uint64_t hi; uint64_t lo;}"))));
    Assert(n->width <= 128, "Data type of memory %s is larger than 128\n", n->name.c_str());
    hfile << n->name << "[" << n->val << "];\n";
  }

  // set functions
  for (Node* node : g->input) {
    if (node->width > 64) {
      hfile << "void set_" << node->name << "(mpz_t val) {\n";
      hfile << "mpz_set(" << node->name << ", val);\n";
    } else {
      hfile << "void set_" << node->name << "(uint64_t val) {\n";
      hfile << node->name << " = val;\n";
    }
    for (Node* next : node->next) hfile << "activeFlags[" << next->id << "] = true;\n";
    hfile << "}\n";
  }
  // step functions
  for (size_t i = 0; i < g->sorted.size(); i++) { hfile << "void step" << g->sorted[i]->id << "();\n"; }

  // functions
  hfile << "void step();\n";
  hfile << "};\n";
  hfile << "#endif\n";
  hfile.close();
}

void genSrc(graph* g, std::string headerFile, std::string srcFile) {
  std::ofstream sfile(std::string(OBJ_DIR) + "/" + srcFile + ".cpp");
  INCLUDE(sfile, headerFile + ".h");
  sfile << "long cycles = 0;\n";
  for (Node* node : g->sorted) {
    // generate function
    Node* activeNode;
    int latency;
    switch (node->type) {
      case NODE_REG_SRC:
      case NODE_REG_DST:
        if (node->insts.size() == 0) continue;
        STEP_START(sfile, g, node);
        sfile << "activeFlags[" << node->id << "] = false;\n";
        if (!node->regNext->regSplit) SET_OLDVAL(sfile, node->regNext);
        DISP_CLUS_INSTS(sfile, node);
        DISP_INSTS(sfile, node);
        Assert(node->type == NODE_REG_DST, "invalid Node %s\n", node->name.c_str());
        if (!node->regNext->regSplit) {
          ACTIVATE(sfile, node, node->regNext->next);
          EMU_LOG(sfile, node->id, OLDNAME(node->width), node);
        } else {
          EMU_LOG(sfile, node->id, node->regNext->name, node);
        }
        break;
      case NODE_OTHERS:
      case NODE_OUT:
      case NODE_L1_RDATA:
        if (node->insts.size() == 0) continue;
        STEP_START(sfile, g, node);
        sfile << "activeFlags[" << node->id << "] = false;\n";
        SET_OLDVAL(sfile, node);
        DISP_CLUS_INSTS(sfile, node);
        DISP_INSTS(sfile, node);
        activeNode = node->type == NODE_REG_DST ? node->regNext : node;
        ACTIVATE(sfile, activeNode, activeNode->next);
        EMU_LOG(sfile, node->id, OLDNAME(node->width), node);
        break;
      case NODE_READER:
        STEP_START(sfile, g, node);
        latency = node->regNext->latency[0];
        if (latency == 0) SET_OLDVAL(sfile, node->member[3]);
        DISP_CLUS_INSTS(sfile, node);
        for (Node* member : node->member) {
          DISP_CLUS_INSTS(sfile, member);
          DISP_INSTS(sfile, member);
        }
        if (latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          MEM_READ(sfile, node->regNext->width, node->regNext->name, node->member[0], node->member[3]);
          ACTIVATE(sfile, node->member[3], node->next);
          EMU_LOG(sfile, node->id, OLDNAME(node->regNext->width), node->member[3]);
          EMU_LOG2(sfile, node->id, node->member[0]);
        }
        break;
      case NODE_WRITER:
        STEP_START(sfile, g, node);
        DISP_CLUS_INSTS(sfile, node);
        for (Node* member : node->member) {
          DISP_CLUS_INSTS(sfile, member);
          DISP_INSTS(sfile, member);
        }
        latency = node->regNext->latency[1];
        if (latency == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          sfile << "if(" << nodeEqualsZero(node->member[1]) << "&&" << nodeEqualsZero(node->member[4]) << ") {\n";
          MEM_WRITE(sfile, node->regNext->width, node->regNext->name, node->member[0], node->member[3]);
          WRITE_LOG(sfile, node->regNext->name, node->member[0]->name, node->member[3]->name, node->width);
          sfile << "}\n";
        }
        EMU_LOG2(sfile, node->id, node->member[0]);
        EMU_LOG2(sfile, node->id, node->member[1]);
        EMU_LOG2(sfile, node->id, node->member[3]);
        EMU_LOG2(sfile, node->id, node->member[4]);
        break;
      case NODE_READWRITER:
        STEP_START(sfile, g, node);
        if (node->regNext->latency[0] == 0) {
          sfile << "if(" << node->member[6]->name << ") ";
          SET_OLDVAL(sfile, node->member[3]);
        }
        DISP_CLUS_INSTS(sfile, node);
        for (Node* member : node->member) {
          DISP_CLUS_INSTS(sfile, member);
          DISP_INSTS(sfile, member);
        }
        // write mode
        sfile << "if(" << node->member[6]->name << "){\n";
        if (node->regNext->latency[1] == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          sfile << "if(" << nodeEqualsZero(node->member[1]) << "&&" << nodeEqualsZero(node->member[5]) << ") {\n";
          MEM_WRITE(sfile, node->regNext->width, node->regNext->name, node->member[0], node->member[4]);
          WRITE_LOG(sfile, node->regNext->name, node->member[0]->name, node->member[4]->name, node->width);
          sfile << "}\n";
        }
        // read mode
        sfile << "} else {\n";
        if (node->regNext->latency[0] == 0) {
          sfile << "activeFlags[" << node->id << "] = false;\n";
          MEM_READ(sfile, node->regNext->width, node->regNext->name, node->member[0], node->member[3]);
          ACTIVATE(sfile, node->member[3], node->next);
          EMU_LOG(sfile, node->id, OLDNAME(node->width), node->member[3]);
        }
        break;
      case NODE_INP: continue;
      default: Assert(0, "Invalid node %s with type %d\n", node->name.c_str(), node->type);
    }
    sfile << "}\n";
  }

  sfile << "" << "void S" << g->name << "::step() {\n";

#ifdef DIFFTEST_PER_SIG
  for (Node* node: g->sources) {
    if (node->regNext->insts.size() == 0) continue;
    if (node->width > 64)
      sfile << "mpz_set(" << node->name << "$prev, " << node->name << ");\n";
    else
      sfile << node->name << "$prev = " << node->name << ";\n";
  }
#endif

  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (g->sorted[i]->insts.size() == 0 && g->sorted[i]->type != NODE_READER && g->sorted[i]->type != NODE_WRITER)
      continue;
    sfile << "if(unlikely(activeFlags[" << g->sorted[i]->id << "])) "
          << "step" << g->sorted[i]->id << "();\n";
  }

  // active nodes
  for (Node* n : g->active) {
    for (size_t i = 0; i < n->insts.size(); i++) sfile << n->insts[i] << ";\n";
  }

  // update registers
  for (Node* node : g->sources) {
    if (node->regNext->status == CONSTANT_NODE || !node->regSplit) continue;
    if (node->next.size()) {
      if (node->width > 64)
        sfile << "if(mpz_cmp(" << node->name << ", " << node->name << "$next)) {\n";
      else
        sfile << "if(" << node->name << " != " << node->name << "$next) {\n";
      for (Node* next : node->next) {
        if (next->status == VALID_NODE && next->type != NODE_ACTIVE) {
          sfile << "activeFlags[" << (next->id == next->clusId ? next->id : next->clusNodes[0]->id) << "] = true;\n";
        }
      }
      sfile << "}\n";
    }

    if (node->width > 64)
      sfile << "mpz_set(" << node->name << ", " << node->name << "$next);\n";
    else
      sfile << node->name << " = " << node->name << "$next;\n";
  }
  // update memory rdata & wdata
  for (Node* node : g->memory) {
    for (Node* rw : node->member) {
      if (rw->type == NODE_READER && node->latency[0] == 1) {
        Node* data = rw->member[3];
        Assert(data->type == NODE_L1_RDATA, "Invalid data type(%d) for reader(%s) with latency 1\n", data->type,
               rw->name.c_str());
        SET_OLDVAL(sfile, data);
        MEM_READ(sfile, node->width, node->name, rw->member[0], data);
        ACTIVATE(sfile, data, data->next);
      } else if (rw->type == NODE_WRITER && node->latency[1] == 1) {
        sfile << "if(" << nodeEqualsZero(rw->member[1]) << " && " << nodeEqualsZero(rw->member[4]) << ") {\n";
        MEM_WRITE(sfile, node->width, node->name, rw->member[0], rw->member[3]);
        for (Node* reader : rw->regNext->member) {
          if (reader->type == NODE_READER || reader->type == NODE_READWRITER)
            sfile << "activeFlags[" << reader->id << "] = true;\n";
        }
        WRITE_LOG(sfile, node->name, rw->member[0]->name, rw->member[3]->name, node->width);
        sfile << "}\n";
      } else if (rw->type == NODE_READWRITER) {
        if (node->latency[0] == 1) {
          Node* data = rw->member[3];
          sfile << "if(!" << rw->member[6]->name << "){\n";
          SET_OLDVAL(sfile, data);
          MEM_READ(sfile, node->width, node->name, rw->member[0], rw->member[3]);
          ACTIVATE(sfile, data, data->next);
          sfile << "}\n";
        }
        if (node->latency[1] == 1) {
          sfile << "if(" << rw->member[6]->name << "){\n";
          sfile << "if(" << nodeEqualsZero(rw->member[1]) << " && " << nodeEqualsZero(rw->member[5]) << ") {\n";
          MEM_WRITE(sfile, node->width, node->name, rw->member[0], rw->member[4]);
          for (Node* reader : rw->regNext->member) {
            if (reader->type == NODE_READER || reader->type == NODE_READWRITER)
              sfile << "activeFlags[" << reader->id << "] = true;\n";
          }
          sfile << "}\n";
        }
      }
    }
  }
  // sfile << "std::cout << \"------\\n\";\n";
  sfile << "cycles ++;\n}\n";
}

void generator(graph* g, std::string header, std::string src) {
  genHeader(g, header);
  genSrc(g, header, src);
  std::cout << "nodeNum " << nodeNum << " " << g->sorted.size() << std::endl;
}
