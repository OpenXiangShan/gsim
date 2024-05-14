/*
  cppEmitter: emit C++ files for simulation
*/

#include "common.h"

#include <cstdio>
#include <map>

#define oldBasic(node) (node->isArrayMember ? arrayMemberOld(node) : (node->name + "$old"))
#define oldMpz(node) std::string("oldValMpz")
#define oldName(node) (node->width > BASIC_WIDTH ? oldMpz(node) : oldBasic(node))

#ifdef DIFFTEST_PER_SIG
FILE* sigFile = nullptr;
#endif
#ifdef EMU_LOG
static int displayNum = 0;
static const int nodePerDisplay = 5000;
#endif
static int superId = 1;
static std::set<Node*> definedNode;

bool nameExist(std::string str);

std::string strRepeat(std::string str, int times) {
  std::string ret;
  for (int i = 0; i < times; i ++) ret += str;
  return ret;
}

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
  Assert(node->isArrayMember, "invalid type %d %s", node->type, node->name.c_str());
  std::string ret = strReplace(node->name, "[", "__");
  ret = strReplace(ret, "]", "__") + "$old";
  return ret;
}

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
static std::string arrayPrevName (std::string name) {
  size_t pos = name.find("[");
  if (pos == name.npos) return name + "$prev";
  std::string ret = name.insert(pos, "$prev");
  return ret;
}
#endif

static void inline declStep(FILE* fp) {
  for (int i = 1; i < superId; i ++) fprintf(fp, "void step%d();\n", i);
  fprintf(fp, "void step();\n");
}

void graph::genMemInit(FILE* fp, Node* node) {
  if (node->width <= BASIC_WIDTH) return;
  std::string idxStr, bracket;
  fprintf(fp, "for (int i = 0; i < %d; i ++) {\n", upperPower2(node->depth));
  for (size_t i = 0; i < node->dimension.size(); i ++) {
    fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i], i);
    idxStr += "[i" + std::to_string(i) + "]";
    bracket += "}\n";
  }
  fprintf(fp, "mpz_init(%s[i]%s);\n", node->name.c_str(), idxStr.c_str());
  fprintf(fp, "%s\n}\n", bracket.c_str());
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
#if  defined (MEM_CHECK) || defined (DIFFTEST_PER_SIG)
    static std::set<Node*> initNodes;
    if (node->isArrayMember) node = node->arrayParent;
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
#ifdef PERF
  fprintf(header, "for (int i = 0; i < %d; i ++) activeTimes[i] = 0;\n", superId);
  fprintf(header, "for (int i = 0; i < %d; i ++) validActive[i] = 0;\n", superId);
#endif
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_init(MPZ_TMP$%d);\n", i);
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    for (Node* member : super->member) {
      genNodeInit(header, member);
    }
  }
  for (Node* mem : memory) {
    genMemInit(header, mem);
  }
  fprintf(header, "mpz_init(oldValMpz);\n");
#if  defined (MEM_CHECK) || defined (DIFFTEST_PER_SIG)
  for (Node* mem :memory) {
    fprintf(header, "memset(%s, 0, sizeof(%s));\n", mem->name.c_str(), mem->name.c_str());
  }
#endif
  fprintf(header, "}\n");

  /* mpz variable used for intermidia values */
  fprintf(header, "mpz_t oldValMpz;\n");
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_t MPZ_TMP$%d;\n", i);

  fprintf(header, "bool activeFlags[%d];\n", superId); // or super.size() if id == idx
#ifdef PERF
  fprintf(header, "size_t activeTimes[%d];\n", superId);
  fprintf(header, "size_t validActive[%d];\n", superId);
#endif
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
  for (std::string inst : input->insts) {
    fprintf(fp, "%s\n", inst.c_str());
  }
  /* TODO: activate node next.size()
    activate all nodes for simplicity
  */
  /* update nodes in the same superNode */
  fprintf(fp, "for (int i = 0; i < %d; i ++) activeFlags[i] = true;\n", superId);
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
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_UPDATE || node->status != VALID_NODE) return;
  if (node->isArrayMember) node = node->arrayParent;
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
    if (node->type == NODE_REG_SRC){
      if (node->width <= BASIC_WIDTH) {
        fprintf(fp, "%s %s%s", widthUType(node->width).c_str(), node->name.c_str(), "$prev");
      } else {
        fprintf(fp, "mpz_t %s%s", node->name.c_str(), "$prev");
      }
      if (node->isArray() && node->arrayEntryNum() != 1) {
        for (int dim : node->dimension) fprintf(fp, "[%d]", dim);
      }
      fprintf(fp, ";\n");
    }
  #endif
  std::string verilatorName = name + "__DOT__" + (node->type == NODE_REG_DST? node->getSrc()->name : node->name);
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
#if defined(VERILATOR_DIFF)
  std::string diffNodeName = node->type == NODE_REG_DST ? (node->getSrc()->name + "$prev") : node->name;
  std::string originName = (node->type == NODE_REG_DST ? node->getSrc()->name : node->name);
#else
  std::string diffNodeName = node->type == NODE_REG_DST ? node->getSrc()->name : node->name;
  std::string originName = (node->type == NODE_REG_DST ? node->getSrc()->name : node->name);
#endif
  if (node->type == NODE_MEMORY){

  } else if (node->isArray() && node->arrayEntryNum() == 1) {
    std::string verilatorSuffix, diffSuffix;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      if (node->type != NODE_REG_DST) diffSuffix += "[0]";
      verilatorSuffix += "_0";
    }
    if (!nameExist(originName + verilatorSuffix) && (!node->arraySplitted() || node->getArrayMember(0)->status == VALID_NODE))
      allNames[diffNodeName + diffSuffix] = verilatorName + verilatorSuffix;
  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    std::vector<std::string> verilatorSuffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            verilatorSuffix[suffixIdx] += "_" + std::to_string(j);
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      if (!nameExist(originName + verilatorSuffix[i])) {
        if (!node->arraySplitted() || node->getArrayMember(i)->name == diffNodeName + suffix[i])
          allNames[diffNodeName + suffix[i]] = verilatorName + verilatorSuffix[i];
      }
    }
  } else {
    allNames[diffNodeName] = verilatorName;
  }
  #ifdef VERILATOR_DIFF
  if (node->type != NODE_REG_SRC) {
    for (auto iter : allNames)
      fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.second.c_str());
  }
  #else
  for (auto iter : allNames)
    fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.first.c_str());
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

std::string graph::saveOldVal(FILE* fp, Node* node) {
  std::string ret;
    /* save oldVal */

  if (!node->isArray()) {
    if (node->width > BASIC_WIDTH) {
      fprintf(fp, "mpz_set(%s, %s);\n", oldMpz(node).c_str(), node->name.c_str());
      ret = oldMpz(node);
    } else {
      fprintf(fp, "%s %s = %s;\n", widthUType(node->width).c_str(), oldBasic(node).c_str(), node->name.c_str());
      ret = oldBasic(node);
    }
  } else if (node->arrayEntryNum() == 1) {
    std::string suffix = strRepeat("[0]", node->dimension.size());
    if (node->width > BASIC_WIDTH) {
      if (node->isArrayMember) TODO();
      fprintf(fp, "mpz_set(%s, %s%s);\n", oldMpz(node).c_str(), node->name.c_str(), suffix.c_str());
      ret = oldMpz(node);
    } else {
      fprintf(fp, "%s %s = %s%s;\n", widthUType(node->width).c_str(), oldBasic(node).c_str(), node->name.c_str(), suffix.c_str());
      ret = oldBasic(node);
    }
  }
  return ret;
}

static void activateNext(FILE* fp, Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep) {
  std::string nodeName = node->name;
  if (node->isArray() && node->arrayEntryNum() == 1) nodeName += strRepeat("[0]", node->dimension.size());
  if (node->width > BASIC_WIDTH) {
    fprintf(fp, "if (mpz_cmp(%s, %s) != 0) {\n", nodeName.c_str(), oldName.c_str());
  } else {
    fprintf(fp, "if (%s != %s) {\n", nodeName.c_str(), oldName.c_str());
  }
  for (int id : nextNodeId) {
    fprintf(fp, "activeFlags[%d] = true;\n", id);
  }
#ifdef PERF
  if (inStep) fprintf(fp, "isActivateValid = true;\n");
#endif
  fprintf(fp, "}\n");
}

static void activateUncondNext(FILE* fp, Node* node, std::set<int>activateId, bool inStep) {
  for (int id : activateId) {
    fprintf(fp, "activeFlags[%d] = true;\n", id);
  }
#ifdef PERF
  if (inStep) fprintf(fp, "isActivateValid = true;\n");
#endif
}

void graph::genNodeInsts(FILE* fp, Node* node) {
  std::string oldnode;
  if (node->insts.size()) {
    /* save oldVal */
    if (node->needActivate()) {
      oldnode = saveOldVal(fp, node);
    }
    /* display all insts */
    for (std::string inst : node->insts) {
      fprintf(fp, "%s\n", inst.c_str());
    }
  }
  if (!node->needActivate()) ;
  else if(node->dimension.size() == 0 || node->arrayEntryNum() == 1) activateNext(fp, node, node->nextActiveId, oldnode, true);
  else activateUncondNext(fp, node, node->nextActiveId, true);
}

void graph::genNodeStepStart(FILE* fp, SuperNode* node) {
  nodeNum ++;
  fprintf(fp, "void S%s::step%d() {\n", name.c_str(), node->cppId);
  if(node->superType == SUPER_SAVE_REG) {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
    fprintf(fp, "saveDiffRegs();\n");
#endif
  } else {
    fprintf(fp, "activeFlags[%d] = false;\n", node->cppId);
  #ifdef PERF
    fprintf(fp, "activeTimes[%d] ++;\n", node->cppId);
    fprintf(fp, "bool isActivateValid = false;\n");
  #endif
  }
}

void graph::nodeDisplay(FILE* fp, SuperNode* super) {
#ifdef EMU_LOG
  int nodeNum = 0;
  fprintf(fp, "void S%s::display%d(){\n", name.c_str(), displayNum ++);
  for (Node* member : super->member) {
    nodeNum ++;
    if (nodeNum == nodePerDisplay) {
      nodeNum = 0;
      fprintf(fp, "}\n");
      fprintf(fp, "void S%s::display%d(){\n", name.c_str(), displayNum ++);
    }
    if (member->status != VALID_NODE) continue;
    fprintf(fp, "if (cycles >= %d && cycles <= %d) {\n", LOG_START, LOG_END);
    if (member->dimension.size() != 0) {
      fprintf(fp, "printf(\"%%ld %d %s: \", cycles);\n", super->cppId, member->name.c_str());
      std::string idxStr, bracket;
      for (size_t i = 0; i < member->dimension.size(); i ++) {
        fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
        idxStr += "[i" + std::to_string(i) + "]";
        bracket += "}\n";
      }
      std::string nameIdx = member->name + idxStr;
      if (member->width > 128) {
        fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", nameIdx.c_str());
      } else if (member->width > 64) {
        fprintf(fp, "printf(\"%%lx|%%lx \", (uint64_t)(%s >> 64), (uint64_t(%s)));", nameIdx.c_str(), nameIdx.c_str());
      } else {
        fprintf(fp, "printf(\"%%lx \", (uint64_t(%s)));", nameIdx.c_str());
      }
      fprintf(fp, "\n%s", bracket.c_str());
      fprintf(fp, "printf(\"\\n\");\n");
    } else if (member->width > 128) {
      fprintf(fp, "printf(\"%%ld %d %s: \", cycles);\n", super->cppId, member->name.c_str());
      fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", member->name.c_str());
      fprintf(fp, "printf(\"\\n\");\n");
    } else if (member->width > 64 && member->width <= 128) {
      if (member->needActivate()) {// display old value and new value
        fprintf(fp, "printf(\"%%ld %d %s %%lx|%%lx \\n\", cycles, (uint64_t)(%s >> 64), (uint64_t(%s)));", super->cppId, member->name.c_str(), member->name.c_str(), member->name.c_str());
      } else if (member->type != NODE_SPECIAL) {
        fprintf(fp, "printf(\"%%ld %d %s %%lx|%%lx \\n\", cycles, (uint64_t)(%s >> 64), (uint64_t(%s)));", super->cppId, member->name.c_str(), member->name.c_str(), member->name.c_str());
      }
    } else {
      if (member->needActivate()) {// display old value and new value
        fprintf(fp, "printf(\"%%ld %d %s %%lx \\n\", cycles, (uint64_t(%s)));", super->cppId, member->name.c_str(), member->name.c_str());
      } else if (member->type != NODE_SPECIAL) {
        fprintf(fp, "printf(\"%%ld %d %s %%lx \\n\", cycles, (uint64_t(%s)));", super->cppId, member->name.c_str(), member->name.c_str());
      }
    }
    fprintf(fp, "}\n");
  }
  fprintf(fp, "}\n");
#endif
}

void graph::genNodeStepEnd(FILE* fp, SuperNode* node) {
#ifdef PERF
  fprintf(fp, "validActive[%d] += isActivateValid;\n", node->cppId);
#endif
#ifdef EMU_LOG
  int num = (node->member.size() + nodePerDisplay - 1) / nodePerDisplay;
  for (int i = 0; i < num; i ++) {
    fprintf(fp, "display%d();\n", displayNum + i);
  }
#endif

  fprintf(fp, "}\n");
  nodeDisplay(fp, node);
}

void graph::genMemRead(FILE* fp) {
  fprintf(fp, "void S%s::updateMem() {\n", name.c_str());
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
          activateUncondNext(fp, port->member[READER_DATA], port->member[READER_DATA]->nextActiveId, false);
        }
      }
    } else {
      for (Node* port : mem->member) {
        if (port->type == NODE_READER && mem->rlatency == 1) {
          if (port->member[READER_DATA]->dimension.size() != 0) {
            fprintf(fp, "memcpy(%s, %s[%s], sizeof(%s));\n", port->member[READER_DATA]->name.c_str(), mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str(), port->member[READER_DATA]->name.c_str());
#ifdef EMU_LOG
            fprintf(fp, "if (cycles >= %d && cycles <= %d) {\n", LOG_START, LOG_END);
            std::string idxStr, bracket;
            fprintf(fp, "printf(\"%%ld read %s \", cycles);\n", port->member[READER_DATA]->name.c_str());
            for (size_t i = 0; i < port->member[READER_DATA]->dimension.size(); i ++) {
              fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, port->member[READER_DATA]->dimension[i], i);
              idxStr += "[i" + std::to_string(i) + "]";
              bracket += "}\n";
            }
            fprintf(fp, "printf(\"%%lx \", (uint64_t(%s)));", (port->member[READER_DATA]->name + idxStr).c_str());
            fprintf(fp, "\n%s", bracket.c_str());
            fprintf(fp, "printf(\"\\n\");\n}\n");
#endif
          }
          else {
            fprintf(fp, "%s = %s[%s];\n", port->member[READER_DATA]->name.c_str(), mem->name.c_str(), port->member[READER_ADDR]->computeInfo->valStr.c_str());
#ifdef EMU_LOG
            fprintf(fp, "if (cycles >= %d && cycles <= %d) {\n", LOG_START, LOG_END);
            fprintf(fp, "printf(\"%%ld read %s %%lx\\n\", cycles, (uint64_t(%s)));\n", port->member[READER_DATA]->name.c_str(), port->member[READER_DATA]->name.c_str());
            fprintf(fp, "}\n");
#endif
          }
          activateUncondNext(fp, port->member[READER_DATA], port->member[READER_DATA]->nextActiveId, false);
        }
      }
    }
  }
  fprintf(fp, "}\n");
}

void graph::genActivate(FILE* fp) {
  int idx = 1;
  for (int i = 0; i < subStepNum; i ++) {
    fprintf(fp, "void S%s::subStep%d(){\n", name.c_str(), i);
    for (int i = 1; i < 10000 && idx < superId; i ++, idx ++) {
      fprintf(fp, "if(unlikely(activeFlags[%d])) step%d();\n", idx, idx);
    }
    fprintf(fp, "}\n");
  }
}

void graph::genUpdateRegister(FILE* fp) {
  /* update registers */
  /* NOTE: register may need to update itself */
  size_t idx = 0;
  for (int i = 0; i < updateRegNum; i ++) {
    fprintf(fp, "void S%s::updateRegister%d() {\n", name.c_str(), i);
    for (int j = 0; j < 10000 && idx < regsrc.size(); j ++) {
      Node* node = regsrc[idx ++];
      if (node->regSplit && node->status == VALID_NODE) {
        if (node->dimension.size() == 0 || node->arrayEntryNum() == 1) {
            std::string regold;
            if (node->regNeedActivate()) regold = saveOldVal(fp, node);
            if (node->updateTree->getRoot()->computeInfo->opNum < 0) {
              fprintf(fp, "%s\n", node->updateTree->getRoot()->computeInfo->valStr.c_str());
            }
            else if (node->width > BASIC_WIDTH) fprintf(fp, "mpz_set(%s, %s);\n", node->updateTree->getlval()->computeInfo->valStr.c_str(), node->updateTree->getRoot()->computeInfo->valStr.c_str());
            else fprintf(fp, "%s = %s;\n", node->updateTree->getlval()->computeInfo->valStr.c_str(), node->updateTree->getRoot()->computeInfo->valStr.c_str());
  #ifdef EMU_LOG
            std::string nodeName = (node->isArray() && node->arrayEntryNum() == 1) ? (node->name + strRepeat("[0]", node->dimension.size())) : node->name;
            if (node->width <= 32)
              fprintf(fp, "if (cycles >= %d && cycles <= %d)\nprintf(\"%%ld reg %s = %%x\\n\", cycles, %s);\n", LOG_START, LOG_END, node->name.c_str(), nodeName.c_str());
            else if (node->width <= 64)
              fprintf(fp, "if (cycles >= %d && cycles <= %d)\nprintf(\"%%ld reg %s = %%lx\\n\", cycles, %s);\n", LOG_START, LOG_END, node->name.c_str(), nodeName.c_str());
  #endif
          if (node->regNeedActivate()) activateNext(fp, node, node->regActivate, regold, false);
        } else {
          // if (node->super->cppId < 0)
            activateUncondNext(fp, node, node->regActivate, false);
          if (node->updateTree->getRoot()->computeInfo->opNum < 0) {
              fprintf(fp, "%s\n", node->updateTree->getRoot()->computeInfo->valStr.c_str());
          } else if (node->width > BASIC_WIDTH) {
            std::string idxStr, bracket;
            for (size_t i = 0; i < node->dimension.size(); i ++) {
              fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i], i);
              idxStr += "[i" + std::to_string(i) + "]";
              bracket += "}\n";
            }
            fprintf(fp, "mpz_set(%s%s, %s%s);\n", node->name.c_str(), idxStr.c_str(), node->updateTree->getRoot()->computeInfo->valStr.c_str(), idxStr.c_str());
            fprintf(fp, "%s", bracket.c_str());
          }
          else fprintf(fp, "memcpy(%s, %s, sizeof(%s));\n", node->name.c_str(),
                    node->updateTree->getRoot()->computeInfo->valStr.c_str(), node->name.c_str());
        }
      }
    }
    fprintf(fp, "}\n");

  }
}

void graph::genMemWrite(FILE* fp) {
  /* update memory*/
  /* writer affects other nodes through reader, no need to activate in writer */
  fprintf(fp, "void S%s::writeMem(){\n", name.c_str());
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
        valInfo* info_en = port->member[WRITER_EN]->computeInfo;
        if (info_en->status == VAL_CONSTANT && mpz_sgn(info_en->consVal) == 0) continue;
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
          std::string cond;
          if (info_en->status != VAL_CONSTANT) cond += port->member[WRITER_EN]->computeInfo->valStr.c_str();
          valInfo* info_mask = port->member[WRITER_MASK]->computeInfo;
          if (info_mask->status != VAL_CONSTANT) cond += (cond.length() == 0 ? "" : " & ") + port->member[WRITER_MASK]->computeInfo->valStr;
          fprintf(fp, "if(%s) {\n", cond.c_str());
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
  fprintf(fp, "}\n");
}

void graph::saveDiffRegs(FILE* fp) {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  fprintf(fp, "void S%s::saveDiffRegs(){\n", name.c_str());
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->type == NODE_REG_SRC && (!member->isArray() || member->arrayEntryNum() == 1) && member->status == VALID_NODE) {
        std::string memberName = member->name;
        if (member->isArray() && member->arrayEntryNum() == 1) {
          for (size_t i = 0; i < member->dimension.size(); i ++) memberName += "[0]";
        }
        if (member->width > BASIC_WIDTH)
          fprintf(fp, "mpz_set(%s, %s);\n", arrayPrevName(member->getSrc()->name).c_str(), memberName.c_str());
        else
          fprintf(fp, "%s = %s;\n", arrayPrevName(member->getSrc()->name).c_str(), memberName.c_str());
      } else if (member->type == NODE_REG_SRC && member->isArray() && member->status == VALID_NODE) {
        std::string idxStr, bracket;
        for (size_t i = 0; i < member->dimension.size(); i ++) {
          fprintf(fp, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
          idxStr += "[i" + std::to_string(i) + "]";
          bracket += "}\n";
        }
        if (member->width > BASIC_WIDTH) {
          fprintf(fp, "mpz_set(%s$prev%s, %s%s);\n", member->name.c_str(), idxStr.c_str(), member->name.c_str(), idxStr.c_str());
        } else {
          fprintf(fp, "%s$prev%s = %s%s;\n", member->name.c_str(), idxStr.c_str(), member->name.c_str(), idxStr.c_str());
        }
        fprintf(fp, "%s", bracket.c_str());
      }
    }
  }
  fprintf(fp, "}\n");
#endif
}

void graph::genStep(FILE* fp) {
  fprintf(fp, "void S%s::step() {\n", name.c_str());
  /* readMemory */
  fprintf(fp, "updateMem();\n");

  for (int i = 0; i < subStepNum; i ++) {
    fprintf(fp, "subStep%d();\n", i);
  }

  fprintf(fp, "writeMem();\n");

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
    if (super->superType == SUPER_SAVE_REG || !super->instsEmpty()) super->cppId = superId ++;
  }
  subStepNum = (superId + 9999) / 10000;
  updateRegNum = (regsrc.size() + 9999) / 10000;
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
    if (super->superType == SUPER_VALID) {
      for (Node* n : super->member) genNodeDef(header, n);
    }
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
#ifdef EMU_LOG
  for (int i = 0; i < displayNum; i ++) {
    fprintf(header, "void display%d();\n", i);
  }
#endif
  for (int i = 0; i < subStepNum; i ++) {
    fprintf(header, "void subStep%d();\n", i);
  }
  fprintf(header, "void updateMem();\n");

  fprintf(header, "void writeMem();\n");

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  fprintf(header, "void saveDiffRegs();\n");
#endif

  /* main evaluation loop (step)*/
  genMemRead(src);
  genActivate(src);
  genMemWrite(src);
  saveDiffRegs(src);
  genStep(src);
  
  genHeaderEnd(header);
  genSrcEnd(src);

  fclose(header);
  fclose(src);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFile);
#endif
}