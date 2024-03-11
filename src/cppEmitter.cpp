/*
  cppEmitter: emit C++ files for simulation
*/

#include "common.h"

#include <map>

#define oldBasic(node) (node->type == NODE_ARRAY_MEMBER ? arrayMemberOld(node) : (node->name + "$old"))
#define oldMpz(node) std::string("oldValMpz")
#define oldName(node) (node->width > BASIC_WIDTH ? oldMpz(node) : oldBasic(node))

#ifdef DIFFTEST_PER_SIG
FILE* sigFile = nullptr;
#endif

static int superId = 1;
static std::set<Node*> definedNode;

static void inline includeLib(FILE* fp, std::string lib, bool isStd) {
  std::string format = isStd ? "#include <%s>\n" : "#include \"%s\"\n";
  fprintf(fp, format.c_str(), lib.c_str());
}

static void inline newLine(FILE* fp) {
  fprintf(fp, "\n");
}

std::string strReplace(std::string s, std::string oldStr, std::string newStr) {
  size_t pos;
  while ((pos = s.find(oldStr)) != std::string::npos) {
    s.replace(pos, oldStr.length(), newStr);
  }
  return s;
}

static std::string arrayMemberOld(Node* node) {
  Assert(node->type == NODE_ARRAY_MEMBER, "invalid type %d %s", node->type, node->name.c_str());
  std::string ret = strReplace(node->name, "[", "__");
  ret = strReplace(ret, "]", "__") + "$old";
  return ret;
}

static void inline declStep(FILE* fp) {
  for (int i = 1; i < superId; i ++) fprintf(fp, "void step%d();\n", i);
  fprintf(fp, "void step();\n");
}

void graph::genNodeInit(FILE* fp, Node* node) {
  if (node->status != VALID_NODE) return;
  if (node->width > BASIC_WIDTH) {
    if (node->isArray()) {
      std::string idxStr, bracket;
      for (size_t i = 0; i < node->dimension.size(); i ++) {
        fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i], i);
        idxStr += "[i" + std::to_string(i) + "]";
        bracket += "}\n";
      }
      fprintf(fp, "mpz_init(%s%s);\n", node->name.c_str(), idxStr.c_str());
      fprintf(fp, "%s", bracket.c_str());
    }
    else
      fprintf(fp, "mpz_init(%s);\n", node->name.c_str());
  } else {
#ifdef MEM_CHECK
    static std::set<Node*> initNodes;
    if (node->type == NODE_ARRAY_MEMBER) node = node->arrayParent;
    if (initNodes.find(node) != initNodes.end()) return;
    initNodes.insert(node);
    switch (node->type) {
      case NODE_INVALID:
      case NODE_SPECIAL:
      case NODE_READER:
      case NODE_WRITER:
      case NODE_READWRITER:
      case NODE_MEMORY:
        break;
      default:
        if (node->isArray()) {
          fprintf(fp, "memset(%s, 0, sizeof(%s));\n", node->name.c_str(), node->name.c_str());
        } else {
          fprintf(fp, "%s = 0;\n", node->name.c_str());
        }
    }
#endif
  }
}

FILE* graph::genHeaderStart(std::string headerFile) {
  FILE* header = std::fopen((std::string(OBJ_DIR) + "/" + headerFile + ".h").c_str(), "w");

  fprintf(header, "#ifndef %s_H\n#define %s_H\n", headerFile.c_str(), headerFile.c_str());
  /* include all libs */
  includeLib(header, "iostream", true);
  includeLib(header, "vector", true);
  includeLib(header, "gmp.h", true);
  includeLib(header, "assert.h", true);
  includeLib(header, "cstdint", true);
  includeLib(header, "ctime", true);
  includeLib(header, "iomanip", true);
  includeLib(header, "cstring", true);
  includeLib(header, "functions.h", false);
  newLine(header);

  fprintf(header, "#define likely(x) __builtin_expect(!!(x), 1)\n");
  fprintf(header, "#define unlikely(x) __builtin_expect(!!(x), 0)\n");
  fprintf(header, "#define uint128_t __uint128_t\n");
  fprintf(header, "#define int128_t __int128_t\n");
  fprintf(header, "#define UINT128(hi, lo) ((uint128_t)(hi) << 64 | (lo))\n");
  newLine(header);
  /* class start*/
  fprintf(header, "class S%s {\npublic:\n", name.c_str());
  fprintf(header, "uint64_t cycles = 0;\n");
  /* constrcutor */
  fprintf(header, "S%s() {\n", name.c_str());
  /* some initialization */
  fprintf(header, "for (int i = 0; i < %d; i ++) activeFlags[i] = true;\n", superId);
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_init(MPZ_TMP$%d);\n", i);
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      genNodeInit(header, member);
    }
  }
  fprintf(header, "mpz_init(oldValMpz);\n");
#ifdef MEM_CHECK
  for (Node* mem :memory) {
    fprintf(header, "memset(%s, 0, sizeof(%s));\n", mem->name.c_str(), mem->name.c_str());
  }
#endif
  fprintf(header, "}\n");

  /* mpz variable used for intermidia values */
  fprintf(header, "mpz_t oldValMpz;\n");
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_t MPZ_TMP$%d;\n", i);

  fprintf(header, "bool activeFlags[%d];\n", superId); // or super.size() if id == idx

  return header;
}

void graph::genInterfaceInput(FILE* fp, Node* input) {
  /* set by string */
  if (input->width > BASIC_WIDTH) {
    fprintf(fp, "void set_%s (std::string val, int base) {\n", input->name.c_str());
    fprintf(fp, "mpz_set_str(%s, val, base);\n", input->name.c_str());
  } else {
    fprintf(fp, "void set_%s(%s val) {\n", input->name.c_str(), widthUType(input->width).c_str());
    fprintf(fp, "%s = val;\n", input->name.c_str());
  }
  /* update nodes in the same superNode */
  if (input->super->cppId > 0)
    fprintf(fp, "activeFlags[%d] = true;\n", input->super->cppId);
  /* update next nodes*/
  for (int nextId : input->nextActiveId) {
    fprintf(fp, "activeFlags[%d] = true;\n", nextId);
  }
  fprintf(fp, "}\n");
}

void graph::genInterfaceOutput(FILE* fp, Node* output) {
  /* TODO: fix constant output which is not exists in sortedsuper */
  if (std::find(sortedSuper.begin(), sortedSuper.end(), output->super) == sortedSuper.end()) return;
  if (output->width > BASIC_WIDTH) {
    fprintf(fp, "std::string get_%s() {\n", output->name.c_str());
    if (output->status == CONSTANT_NODE) fprintf(fp, "return \"0x%s\";\n", output->computeInfo->valStr.c_str());
    else fprintf(fp, "return std::string(\"0x\") + mpz_get_str(NULL, 16, %s);\n", output->computeInfo->valStr.c_str());
  } else {
    fprintf(fp, "%s get_%s() {\n", widthUType(output->width).c_str(), output->name.c_str());
    if (output->status == CONSTANT_NODE) fprintf(fp, "return 0x%s;\n", output->computeInfo->valStr.c_str());
    else fprintf(fp, "return %s;\n", output->computeInfo->valStr.c_str());
  }
  fprintf(fp, "}\n");
}

void graph::genHeaderEnd(FILE* fp) {

  fprintf(fp, "};\n");
  fprintf(fp, "#endif\n");
}

void graph::genNodeDef(FILE* fp, Node* node) {
  if (node->type == NODE_SPECIAL || node->status != VALID_NODE) return;
  if (node->type == NODE_ARRAY_MEMBER) node = node->arrayParent;
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  if (node->width <= BASIC_WIDTH) {
    fprintf(fp, "%s %s", widthUType(node->width).c_str(), node->name.c_str());
  } else {
    fprintf(fp, "mpz_t %s", node->name.c_str());
  }
  if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
  for (int dim : node->dimension) fprintf(fp, "[%d]", dim);
  fprintf(fp, ";\n");
#ifdef DIFFTEST_PER_SIG
  #if defined(VERILATOR_DIFF)
    if (node->type == NODE_REG_DST && !node->isArray()){
      if (node->width <= BASIC_WIDTH) {
        fprintf(fp, "%s %s%s;\n", widthUType(node->width).c_str(), node->getSrc()->name.c_str(), "$prev");
      } else {
        fprintf(fp, "mpz_t %s%s;\n", node->getSrc()->name.c_str(), "$prev");
      }
    }
  #endif
  #ifdef VERILATOR_DIFF
  std::string verilatorName = name + "__DOT__" + (node->type == NODE_REG_DST? node->getSrc()->name : node->name);
  #else
  std::string verilatorName = name + "__DOT__" + node->name;
  #endif
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  #ifdef VERILATOR_DIFF
  if (node->type != NODE_REG_SRC) fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, node->type == NODE_REG_DST ? (node->getSrc()->name + "$prev").c_str() : node->name.c_str(), verilatorName.c_str());
  #else
  fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, node->name.c_str(), verilatorName.c_str());
  #endif
#endif
}

FILE* graph::genSrcStart(std::string name) {
  FILE* src = std::fopen((std::string(OBJ_DIR) + "/" + name + ".cpp").c_str(), "w");
  includeLib(src, name + ".h", false);

  return src;
}

void graph::genSrcEnd(FILE* fp) {

}

void graph::genNodeInsts(FILE* fp, Node* node) {
  std::string ret;
  if (node->insts.size()) {
    /* save oldVal */
    if (node->needActivate()) {
      if (!node->isArray()) {
        if (node->width > BASIC_WIDTH) {
          if (node->type == NODE_ARRAY_MEMBER) TODO();
          fprintf(fp, "mpz_set(%s, %s);\n", oldMpz(node).c_str(), node->name.c_str());
        } else {
          fprintf(fp, "%s %s = %s;\n", widthUType(node->width).c_str(), oldBasic(node).c_str(), node->name.c_str());
        }
      } else if (node->isFakeArray()) {
        if (node->width > BASIC_WIDTH) {
          if (node->type == NODE_ARRAY_MEMBER) TODO();
          fprintf(fp, "mpz_set(%s, %s[0]);\n", oldMpz(node).c_str(), node->name.c_str());
        } else {
          fprintf(fp, "%s %s = %s[0];\n", widthUType(node->width).c_str(), oldBasic(node).c_str(), node->name.c_str());
        }
      }
    }
    /* display all insts */
    for (std::string inst : node->insts) {
      // ret += inst + "\n";
      fprintf(fp, "%s\n", inst.c_str());
    }
  }
}

static void activateNext(FILE* fp, Node* node, std::string oldName, std::string suffix) {
  if (!node->needActivate()) return;
  if (node->width > BASIC_WIDTH) {
    fprintf(fp, "if (mpz_cmp(%s%s, %s) != 0) {\n", node->name.c_str(), suffix.c_str(), oldName.c_str());
  } else {
    fprintf(fp, "if (%s%s != %s) {\n", node->name.c_str(), suffix.c_str(), oldName.c_str());
  }
  for (int id : node->nextActiveId) {
    fprintf(fp, "activeFlags[%d] = true;\n", id);
  }
  fprintf(fp, "}\n");
}

static void activateUncondNext(FILE* fp, Node* node) {
  for (int id : node->nextActiveId) {
    fprintf(fp, "activeFlags[%d] = true;\n", id);
  }
}

void graph::genNodeStepStart(FILE* fp, SuperNode* node) {
  nodeNum ++;
  fprintf(fp, "void S%s::step%d() {\n", name.c_str(), node->cppId);
  fprintf(fp, "activeFlags[%d] = false;\n", node->cppId);
}

void graph::genNodeStepEnd(FILE* fp, SuperNode* node) {
  for (Node* member : node->member) {
    if(member->dimension.size() == 0) activateNext(fp, member, oldName(member), "");
    else if (member->isFakeArray()) activateNext(fp, member, oldName(member), "[0]");
    else activateUncondNext(fp, member);
  }
#ifdef EMU_LOG
  for (Node* member : node->member) {
    if (member->status != VALID_NODE) continue;
    fprintf(fp, "if (cycles >= %d) {\n", LOG_START);
    if (member->dimension.size() != 0) {
      fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\" ;\n", node->cppId, member->name.c_str());
      std::string idxStr, bracket;
      for (size_t i = 0; i < member->dimension.size(); i ++) {
        fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
        idxStr += "[i" + std::to_string(i) + "]";
        bracket += "}\n";
      }
      std::string nameIdx = member->name + idxStr;
      if (member->width > 128) {
        fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", nameIdx.c_str());
      } else if (member->width > 64)
        fprintf(fp, "std::cout << std::hex << +(uint64_t)(%s >> 64) << +(uint64_t)(%s) << \" \";", nameIdx.c_str(), nameIdx.c_str());
      else
        fprintf(fp, "std::cout << std::hex << +(uint64_t)%s << \" \";", nameIdx.c_str());
      fprintf(fp, "\n%s", bracket.c_str());
      fprintf(fp, "std::cout << std::endl;\n");
    } else if (member->width > 128) {
      fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\";\n", node->cppId, member->name.c_str());
      if (member->needActivate()) {
        fprintf(fp, "mpz_out_str(stdout, 16, oldValMpz);\nstd::cout << \" -> \";\n");
      }
      fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", member->name.c_str());
      fprintf(fp, "std::cout << std::endl;\n");
    } else if (member->width > 64 && member->width <= 128) {
      if (member->needActivate()) // display old value and new value
        fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\" << std::hex << (uint64_t)(%s >> 64) << \" \" << (uint64_t)%s  << \
                    \" -> \" << (uint64_t)(%s >> 64) << \" \" << (uint64_t)%s << std::endl;\n",
                node->cppId, member->name.c_str(), oldName(member).c_str(), oldName(member).c_str(), member->name.c_str(), member->name.c_str());
      else if (member->type != NODE_SPECIAL) {
        fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\" << std::hex << (uint64_t)(%s >> 64) << \" \" << (uint64_t)%s << std::endl;\n",
            node->cppId, member->name.c_str(), member->name.c_str(), member->name.c_str());
      }
    } else {
      if (member->needActivate()) // display old value and new value
        fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\" << std::hex << +%s << \" -> \" << +%s << std::endl;\n", node->cppId, member->name.c_str(), oldName(member).c_str(), member->name.c_str());
      else if (member->type != NODE_SPECIAL) {
        fprintf(fp, "std::cout << cycles << \" \" << %d << \" %s :\" << std::hex << +%s << std::endl;\n", node->cppId, member->name.c_str(), member->name.c_str());
      }
    }
    fprintf(fp, "}\n");
  }
#endif

  fprintf(fp, "}\n");
}

void graph::genStep(FILE* fp) {
  fprintf(fp, "void S%s::step() {\n", name.c_str());
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->type == NODE_REG_DST && !member->isArray() && member->status == VALID_NODE) {
        if (member->width > BASIC_WIDTH)
          fprintf(fp, "mpz_set(%s%s, %s);\n", member->getSrc()->name.c_str(), "$prev", member->name.c_str());
        else
          fprintf(fp, "%s%s = %s;\n", member->getSrc()->name.c_str(), "$prev", member->name.c_str());
      }
    }
  }
#endif
  /* readMemory */
  for (Node* mem : memory) {
    Assert(mem->rlatency <= 1 && mem->wlatency == 1, "rlatency %d wlatency %d in mem %s\n", mem->rlatency, mem->wlatency, mem->name.c_str());
    if (mem->width > BASIC_WIDTH) {
      for (Node* port : mem->member) {
        if (port->type == NODE_READER && mem->rlatency == 1) {
          if (port->member[READER_DATA]->dimension.size() != 0) {
             std::string indexStr, bracket;
            for (size_t i = 0; i < mem->dimension.size(); i ++) {
              fprintf(fp, "for (int i%ld = 0; i%ld < %d; i%ld ++) {", i, i, mem->dimension[i], i);
              indexStr += format("[i%ld]", i);
              bracket += "}\n";
            }
            fprintf(fp, "mpz_set(%s%s, %s[%s]%s);\n", port->member[READER_DATA]->computeInfo->valStr.c_str(), indexStr.c_str(),
                                mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str(), indexStr.c_str());

            fprintf(fp, "%s", bracket.c_str());
          } else {
            fprintf(fp, "mpz_set(%s, %s[%s]);\n", port->member[READER_DATA]->name.c_str(), mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str());
          }
          activateUncondNext(fp, port->member[READER_DATA]);
        }
      }
    } else {
      for (Node* port : mem->member) {
        if (port->type == NODE_READER && mem->rlatency == 1) {
          if (port->member[READER_DATA]->dimension.size() != 0)
            fprintf(fp, "memcpy(%s, %s[%s], sizeof(%s));\n", port->member[READER_DATA]->name.c_str(), mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str(), port->member[READER_DATA]->name.c_str());
          else {
            fprintf(fp, "%s = %s[%s];\n", port->member[READER_DATA]->name.c_str(), mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str());
          }
          activateUncondNext(fp, port->member[READER_DATA]);
        }
      }
    }
  }
  /* TODO: may sth special for printf or printf always activate itself(or when cond is satisfied )*/
  for (int i = 1; i < superId; i ++) {
    fprintf(fp, "if(unlikely(activeFlags[%d])) step%d();\n", i, i);
  }
  /* update registers */
  /* NOTE: register may need to update itself */
  for (Node* node : regsrc) {
    if (node->regSplit && node->status == VALID_NODE) {
      if (node->dimension.size() == 0) {
        activateNext(fp, node, node->regNext->computeInfo->valStr, "");
        if (node->width > BASIC_WIDTH) fprintf(fp, "mpz_set(%s, %s);\n", node->name.c_str(), node->regNext->computeInfo->valStr.c_str());
        else fprintf(fp, "%s = %s;\n", node->name.c_str(), node->regNext->computeInfo->valStr.c_str());
      } else {
        activateUncondNext(fp, node);
        if (node->width > BASIC_WIDTH) {
          std::string idxStr, bracket;
          for (size_t i = 0; i < node->dimension.size(); i ++) {
            fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i], i);
            idxStr += "[i" + std::to_string(i) + "]";
            bracket += "}\n";
          }
          fprintf(fp, "mpz_set(%s%s, %s%s);\n", node->name.c_str(), idxStr.c_str(), node->regNext->computeInfo->valStr.c_str(), idxStr.c_str());
          fprintf(fp, "%s", bracket.c_str());
        }
        else fprintf(fp, "memcpy(%s, %s, sizeof(%s));\n", node->name.c_str(), node->regNext->computeInfo->valStr.c_str(), node->name.c_str());
      }
    }
  }
  /* update memory*/
  /* writer affects other nodes through reader, no need to activate in writer */
  for (Node* mem : memory) {
    std::set<SuperNode*> readerL0;
    if (mem->rlatency == 0) {
      for (Node* port : mem->member) {
        if (port->type == NODE_READER) readerL0.insert(port->get_member(READER_DATA)->super);
      }
    }
    Assert(mem->rlatency <= 1 && mem->wlatency == 1, "rlatency %d wlatency %d in mem %s\n", mem->rlatency, mem->wlatency, mem->name.c_str());
    for (Node* port : mem->member) {
      if (port->type == NODE_WRITER) {
        if (port->member[WRITER_DATA]->isArray() != 0) {
          Node* writerData = port->member[WRITER_DATA];
          fprintf(fp, "if(%s) {\n", port->member[WRITER_EN]->computeInfo->valStr.c_str());
          std::string indexStr, bracket;
          for (size_t i = 0; i < writerData->dimension.size(); i ++) {
            fprintf(fp, "for (int i%ld = 0; i%ld < %d; i%ld ++) {", i, i, writerData->dimension[i], i);
            indexStr += format("[i%ld]", i);
            bracket += "}\n";
          }
          if (mem->width > BASIC_WIDTH) {
            fprintf(fp, "if (%s%s) mpz_set(%s[%s]%s, %s%s);\n", port->member[WRITER_MASK]->computeInfo->valStr.c_str(), indexStr.c_str(),
                                          mem->name.c_str(), port->member[WRITER_ADDR]->computeInfo->valStr.c_str(), indexStr.c_str(),
                                          port->member[WRITER_DATA]->computeInfo->valStr.c_str(), indexStr.c_str());
          } else {
            fprintf(fp, "if (%s%s) %s[%s]%s = %s%s;\n", port->member[WRITER_MASK]->computeInfo->valStr.c_str(), indexStr.c_str(),
                                                    mem->name.c_str(), port->member[WRITER_ADDR]->computeInfo->valStr.c_str(), indexStr.c_str(),
                                                    port->member[WRITER_DATA]->computeInfo->valStr.c_str(), indexStr.c_str());
          }
          fprintf(fp, "%s", bracket.c_str());
        } else {
          fprintf(fp, "if(%s && %s) {\n", port->member[WRITER_EN]->computeInfo->valStr.c_str(), port->member[WRITER_MASK]->computeInfo->valStr.c_str());
          if (mem->width > BASIC_WIDTH) {
            if (port->member[WRITER_DATA]->computeInfo->status == VAL_CONSTANT) {
              if (mpz_cmp_ui(port->member[WRITER_DATA]->computeInfo->consVal, MAX_U64) > 0) TODO();
              else fprintf(fp, "mpz_set_ui(%s[%s], %s);\n", mem->name.c_str(), port->member[WRITER_ADDR]->computeInfo->valStr.c_str(), port->member[WRITER_DATA]->computeInfo->valStr.c_str());
            } else {
              fprintf(fp, "mpz_set(%s[%s], %s);\n", mem->name.c_str(), port->member[WRITER_ADDR]->computeInfo->valStr.c_str(), port->member[WRITER_DATA]->computeInfo->valStr.c_str());
            }
          } else {
            fprintf(fp, "%s[%s] = %s;\n", mem->name.c_str(), port->member[WRITER_ADDR]->computeInfo->valStr.c_str(), port->member[WRITER_DATA]->computeInfo->valStr.c_str());
          }
        }
        for (SuperNode* super : readerL0) {
          if (super->cppId <= 0) continue;
          fprintf(fp, "activeFlags[%d] = true;\n", super->cppId);
        }
        fprintf(fp, "}\n");

      } else if (port->type == NODE_READWRITER) {
        TODO();
      }
    }
  }

  fprintf(fp, "cycles ++;\n");
  fprintf(fp, "}\n");
}

bool SuperNode::instsEmpty() {
  for (Node* n : member) {
    if (n->insts.size() != 0) return false;
  }
  return true;
}


void graph::cppEmitter() {
  for (SuperNode* super : sortedSuper) {
    if (!super->instsEmpty()) super->cppId = superId ++;
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE)
        member->updateActivate();
    }
  }

  FILE* header = genHeaderStart(name);
  FILE* src = genSrcStart(name);
  // header: node definition; src: node evaluation
#ifdef DIFFTEST_PER_SIG
  sigFile = fopen((std::string(OBJ_DIR) + "/" + name + "_sigs.txt").c_str(), "w");
#endif

  for (SuperNode* super : sortedSuper) {
    // std::string insts;
    for (Node* n : super->member) genNodeDef(header, n);
    if (super->cppId <= 0) continue;
    genNodeStepStart(src, super);
    for (Node* n : super->member) {
      // genNodeDef(header, n);
      genNodeInsts(src, n);
    }
    genNodeStepEnd(src, super);
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDef(header, mem);
   /* input/output interface */
  for (Node* node : input) genInterfaceInput(header, node);
  for (Node* node : output) genInterfaceOutput(header, node);
  /* declare step functions */
  declStep(header);
  /* main evaluation loop (step)*/
  genStep(src);
  
  genHeaderEnd(header);
  genSrcEnd(src);

  fclose(header);
  fclose(src);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFile);
#endif
}