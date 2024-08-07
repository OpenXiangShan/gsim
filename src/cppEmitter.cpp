/*
  cppEmitter: emit C++ files for simulation
*/

#include "common.h"

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

#define ACTIVE_WIDTH 8
#define NODE_PER_SUBFUNC 2400

#ifdef DIFFTEST_PER_SIG
FILE* sigFile = nullptr;
#endif
#ifdef EMU_LOG
static int displayNum = 0;
#endif

#define RESET_NAME(node) (node->name + "$RESET")

static int superId = 0;
static int activeFlagNum = 0;
static std::set<Node*> definedNode;
static std::map<int, SuperNode*> cppId2Super;
extern std::set<std::pair<int, int>> allMask;
bool nameExist(std::string str);

std::pair<int, int> cppId2flagIdx(int cppId) {
  int id = cppId / ACTIVE_WIDTH;
  int bit = cppId % ACTIVE_WIDTH;
  return std::make_pair(id, bit);
}

std::pair<int, uint64_t>setIdxMask(int cppId) {
  int id, bit;
  std::tie(id, bit) = cppId2flagIdx(cppId);
  uint64_t mask = (uint64_t)1 << bit;
  return std::make_pair(id, mask);
}

std::pair<int, uint64_t>clearIdxMask(int cppId) {
  int id, bit;
  std::tie(id, bit) = cppId2flagIdx(cppId);
  uint64_t mask = (uint64_t)1 << bit;
  if (ACTIVE_WIDTH == 64) mask = ~mask;
  else mask = (~mask) & (((uint64_t)1 << ACTIVE_WIDTH) - 1);
  return std::make_pair(id, mask);
}

std::pair<uint64_t, std::string> activeSet2bitMap(std::set<int>& activeId, std::map<uint64_t, std::pair<uint64_t, std::string>>& bitMapInfo, int curId) {
  uint64_t ret = 0;
  std::string comment = "";
  for (int id : activeId) {
    int bitMapId;
    uint64_t bitMapMask;
    std::tie(bitMapId, bitMapMask) = setIdxMask(id);
    int num = 64 / ACTIVE_WIDTH;
    bool find = false;
    if (curId >= 0 && id > curId && bitMapId == curId / ACTIVE_WIDTH) {
      ret |= bitMapMask;
      comment += " " + std::to_string(id);
    } else {
      for (int i = 0; i < num; i ++) {
        int newId = bitMapId - i;
        if (bitMapInfo.find(newId) != bitMapInfo.end()) {
          bitMapInfo[newId].first |= bitMapMask << (i * ACTIVE_WIDTH);
          bitMapInfo[newId].second += " " + std::to_string(id);
          find = true;
        }
      }
      if (!find) bitMapInfo[bitMapId] = std::make_pair(bitMapMask, std::to_string(id));
    }
  }
  return std::make_pair(ret, comment);
}

std::string updateActiveStr(int idx, uint64_t mask) {
  if (mask <= MAX_U8) return format("activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U16) return format("*(uint16_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U32) return format("*(uint32_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  return format("*(uint64_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
}

std::string updateActiveStr(int idx, uint64_t mask, std::string& cond) {
  auto activeFlags = std::string("activeFlags[") + std::to_string(idx) + std::string("]");

  if (mask <= MAX_U8)
    return format("%s |= %s ?  0x%lx : %s;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  if (mask <= MAX_U16)
    return format("*(uint16_t*)&%s |= %s ? 0x%lx : %s;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  if (mask <= MAX_U32)
    return format("*(uint32_t*)&%s |= %s ? 0x%lx : %s;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  return format("*(uint64_t*)&%s |= %s ?  0x%lx : %s;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
}

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

std::string arrayMemberName(Node* node, std::string suffix) {
  Assert(node->isArrayMember, "invalid type %d %s", node->type, node->name.c_str());
  std::string ret = strReplace(node->name, "[", "__");
  ret = strReplace(ret, "]", "__") + "$" + suffix;
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
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_UPDATE || node->status != VALID_NODE) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
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
    if (initNodes.find(node) != initNodes.end()) return;
    if (node->type == NODE_OTHERS && !node->needActivate() && node->width <= BASIC_WIDTH && !node->isArray()) return;
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
  for (std::string inst : node->initInsts) fprintf(fp, "%s\n", strReplace(inst, ASSIGN_LABLE, "").c_str());
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
  includeLib(header, "map", true);
  includeLib(header, "functions.h", false);
  newLine(header);

  fprintf(header, "#define likely(x) __builtin_expect(!!(x), 1)\n");
  fprintf(header, "#define unlikely(x) __builtin_expect(!!(x), 0)\n");
  fprintf(header, "#define uint128_t __uint128_t\n");
  fprintf(header, "#define int128_t __int128_t\n");
  fprintf(header, "#define uint256_t unsigned _BitInt(256)\n");
  fprintf(header, "#define int256_t _BitInt(256)\n");
  fprintf(header, "#define UINT128(hi, lo) ((uint128_t)(hi) << 64 | (lo))\n");
  fprintf(header, "#define UINT256_3(_2, _1, _0) ((uint256_t)(_2) << 128 | (uint256_t)(_1) << 64 | (_0))\n");
  fprintf(header, "#define UINT256_4(_3, _2, _1, _0) ((uint256_t)(_3) << 192 | UINT256_3(_2, _1, _0))\n");
  newLine(header);
  /* class start*/
  fprintf(header, "class S%s {\npublic:\n", name.c_str());
  fprintf(header, "uint64_t cycles = 0;\n");
  /* constrcutor */
  fprintf(header, "S%s() {\n", name.c_str());
  /* some initialization */
  fprintf(header, "activateAll();\n");
#ifdef PERF
  fprintf(header, "for (int i = 0; i < %d; i ++) activeTimes[i] = 0;\n", superId);
  fprintf(header, "for (int i = 0; i < %d; i ++) activator[i] = std::map<int, int>();\n", superId);
for (SuperNode* super : sortedSuper) {
  if (super->cppId >= 0) fprintf(header, "nodeNum[%d] = %ld;\n", super->cppId, super->member.size());
}
  fprintf(header, "for (int i = 0; i < %d; i ++) validActive[i] = 0;\n", superId);
#endif
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_init(MPZ_TMP$%d);\n", i);
  for (auto range : allMask) {
    std::string maskName = format("MPZ_MASK$%d_%d", range.first, range.second);
    fprintf(header, "mpz_init(%s);\n", maskName.c_str());
    fprintf(header, "mpz_set_ui(%s, 1);\n", maskName.c_str());
    fprintf(header, "mpz_mul_2exp(%s, %s, %d);\n", maskName.c_str(), maskName.c_str(), range.first - range.second + 1);
    fprintf(header, "mpz_sub_ui(%s, %s, 1);\n", maskName.c_str(), maskName.c_str());
    if (range.second != 0)
      fprintf(header, "mpz_mul_2exp(%s, %s, %d);\n", maskName.c_str(), maskName.c_str(), range.second);
  }
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID && super->superType != SUPER_ASYNC_RESET) continue;
    for (Node* member : super->member) {
      genNodeInit(header, member);
    }
  }
  for (Node* mem : memory) {
    genMemInit(header, mem);
  }
  fprintf(header, "mpz_init(newValMpz);\n");
#if  defined (MEM_CHECK) || defined (DIFFTEST_PER_SIG)
  for (Node* mem :memory) {
    fprintf(header, "memset(%s, 0, sizeof(%s));\n", mem->name.c_str(), mem->name.c_str());
  }
#endif
  fprintf(header, "}\n");

  fprintf(header, "void activateAll() {\n");
  fprintf(header, "for (int i = 0; i < %d; i ++) activeFlags[i] = -1;\n", activeFlagNum);
  fprintf(header, "}\n");

  /* mpz variable used for intermidia values */
  fprintf(header, "mpz_t newValMpz;\n");
  for (int i = 0; i < maxTmp; i ++) fprintf(header, "mpz_t MPZ_TMP$%d;\n", i);
  for (auto range : allMask) fprintf(header, "mpz_t MPZ_MASK$%d_%d;\n", range.first, range.second);
  fprintf(header, "uint%d_t activeFlags[%d];\n", ACTIVE_WIDTH, activeFlagNum); // or super.size() if id == idx
#ifdef PERF
  fprintf(header, "size_t activeTimes[%d];\n", superId);
  fprintf(header, "std::map<int, int>activator[%d];\n", superId);
  fprintf(header, "size_t validActive[%d];\n", superId);
  fprintf(header, "size_t nodeNum[%d];\n", superId);
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
  fprintf(fp, "for (int i = 0; i < %d; i ++) activeFlags[i] = -1;\n", activeFlagNum);
  fprintf(fp, "}\n");
}

void graph::genInterfaceOutput(FILE* fp, Node* output) {
  /* TODO: fix constant output which is not exists in sortedsuper */
  if (std::find(sortedSuper.begin(), sortedSuper.end(), output->super) == sortedSuper.end()) return;
  if (output->width > BASIC_WIDTH) {
    fprintf(fp, "std::string get_%s() {\n", output->name.c_str());
    if (output->status == CONSTANT_NODE) fprintf(fp, "return \"%s\";\n", output->computeInfo->valStr.c_str());
    else fprintf(fp, "return std::string(\"0x\") + mpz_get_str(NULL, 16, %s);\n", output->computeInfo->valStr.c_str());
  } else {
    fprintf(fp, "%s get_%s() {\n", widthUType(output->width).c_str(), output->name.c_str());
    if (output->status == CONSTANT_NODE) fprintf(fp, "return %s;\n", output->computeInfo->valStr.c_str());
    else fprintf(fp, "return %s;\n", output->computeInfo->valStr.c_str());
  }
  fprintf(fp, "}\n");
}

void graph::genHeaderEnd(FILE* fp) {

  fprintf(fp, "};\n");
  fprintf(fp, "#endif\n");
}

#if defined(DIFFTEST_PER_SIG) && defined(GSIM_DIFF)
void graph::genDiffSig(FILE* fp, Node* node) {
  std::set<std::string> allNames;
  std::string diffNodeName = node->name;
  std::string originName = node->name;
  if (node->type == NODE_MEMORY){

  } else if (node->isArrayMember) {
    allNames.insert(node->name);
  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      if (!node->arraySplitted() || node->getArrayMember(i)->name == diffNodeName + suffix[i])
        allNames.insert(diffNodeName + suffix[i]);
    }
  } else {
    allNames.insert(diffNodeName);
  }
  for (auto iter : allNames)
    fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.c_str(), iter.c_str());
}
#endif

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
void graph::genDiffSig(FILE* fp, Node* node) {
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
  std::string verilatorName = name + "__DOT__" + (node->type == NODE_REG_DST? node->getSrc()->name : node->name);
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
  std::string diffNodeName = node->type == NODE_REG_DST ? (node->getSrc()->name + "$prev") : node->name;
  std::string originName = (node->type == NODE_REG_DST ? node->getSrc()->name : node->name);
  if (node->type == NODE_MEMORY || node->type == NODE_REG_DST){

  } else if (node->isArrayMember) {
    allNames[node->name] = node->name;
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
  if (node->type != NODE_REG_SRC) {
    for (auto iter : allNames)
      fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.second.c_str());
  }
}
#endif

void graph::genNodeDef(FILE* fp, Node* node) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_UPDATE || node->status != VALID_NODE) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_OTHERS && !node->needActivate() && node->width <= BASIC_WIDTH && !node->isArray()) return;
#ifdef GSIM_DIFF
  genDiffSig(fp, node);
#endif
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  if (node->width <= BASIC_WIDTH) {
    fprintf(fp, "%s %s", widthUType(node->width).c_str(), node->name.c_str());
  } else {
    fprintf(fp, "mpz_t %s", node->name.c_str());
  }
  if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
  for (int dim : node->dimension) fprintf(fp, "[%d]", dim);
#ifdef VERILATOR_DIFF
  genDiffSig(fp, node);
#endif
  fprintf(fp, "; // width = %d\n", node->width);
  /* save reset registers */
  if (node->isReset() && node->type == NODE_REG_SRC) {
    Assert(!node->isArray() && node->width <= BASIC_WIDTH, "%s is treated as reset (isArray: %d width: %d)", node->name.c_str(), node->isArray(), node->width);
    fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), RESET_NAME(node).c_str());
  }

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
  if (node->isArray()) return ret;
    /* save oldVal */
  if (node->fullyUpdated) {
    if (node->width <= BASIC_WIDTH) {
      fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), newBasic(node).c_str());
      ret = newBasic(node);
    }
  } else {
    if (node->width > BASIC_WIDTH) {
      fprintf(fp, "mpz_set(%s, %s);\n", newMpz(node).c_str(), node->name.c_str());
      ret = newMpz(node);
    } else {
      fprintf(fp, "%s %s = %s;\n", widthUType(node->width).c_str(), newBasic(node).c_str(), node->name.c_str());
      ret = newBasic(node);
    }
  }
  return ret;
}

static void activateNext(FILE* fp, Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep) {
  std::string nodeName = node->name;
  auto condName = std::string("cond_") + nodeName;
  bool opt{false};

  if (node->isArray() && node->arrayEntryNum() == 1) nodeName += strRepeat("[0]", node->dimension.size());
  if (node->isAsyncReset()) {
    if (node->width > BASIC_WIDTH) {
      fprintf(fp, "if (mpz_sgn(%s) != 0 || mpz_cmp(%s, %s) != 0) {\n", oldName.c_str(), nodeName.c_str(), oldName.c_str());
    } else {
      fprintf(fp, "if (%s || (%s != %s)) {\n", oldName.c_str(), nodeName.c_str(), oldName.c_str());
    }
  } else {
    if (node->width > BASIC_WIDTH) {
      fprintf(fp, "if (mpz_cmp(%s, %s) != 0) {\n", nodeName.c_str(), oldName.c_str());
    } else {
      opt = true;
      fprintf(fp, "auto %s = %s != %s;", condName.c_str(), nodeName.c_str(), oldName.c_str());
    }
  }
  if (inStep) {
    if (node->width > BASIC_WIDTH)
      fprintf(fp, "mpz_set(%s, %s);\n", node->name.c_str(), newName(node).c_str());
    else {
      if (node->isReset() && node->type == NODE_REG_SRC) fprintf(fp, "%s = %s;\n", RESET_NAME(node).c_str(), newName(node).c_str());
      fprintf(fp, "%s = %s ? %s : %s;\n", node->name.c_str(), condName.c_str(), newName(node).c_str(),
              nodeName.c_str());
    }
  }
  if (node->isAsyncReset()) {
    fprintf(fp, "activateAll();\n");
    fprintf(fp, "oldFlag = -1;\n");
  }
  else {
    std::map<uint64_t, std::pair<uint64_t, std::string>> bitMapInfo;
    std::pair<uint64_t, std::string> curMask = activeSet2bitMap(nextNodeId, bitMapInfo, node->super->cppId);
    if (curMask.first != 0) fprintf(fp, "oldFlag |= %s ?  0x%lx : oldFlag; // %s\n", condName.c_str() ,curMask.first, curMask.second.c_str());
    for (auto iter : bitMapInfo) {
      auto str = opt ? updateActiveStr(iter.first, iter.second.first, condName) : updateActiveStr(iter.first, iter.second.first);
      fprintf(fp, "%s // %s\n", str.c_str(), iter.second.second.c_str());
    }
  #ifdef PERF
    for (int id : nextNodeId) {
      fprintf(fp, "if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\nactivator[%d][%d] ++;\n",
                  id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
    }
    if (inStep) fprintf(fp, "isActivateValid = true;\n");
  #endif
  }
  if (!opt) fprintf(fp, "}\n");
}

static void activateUncondNext(FILE* fp, Node* node, std::set<int>activateId, bool inStep) {
  if (!node->fullyUpdated) fprintf(fp, "if (%s) {\n", ASSIGN_INDI(node).c_str());
  std::map<uint64_t, std::pair<uint64_t, std::string>> bitMapInfo;
  auto curMask = activeSet2bitMap(activateId, bitMapInfo, node->super->cppId);
  if (curMask.first != 0) fprintf(fp, "oldFlag |= 0x%lx; // %s\n", curMask.first, curMask.second.c_str());
  for (auto iter : bitMapInfo) {
    fprintf(fp, "%s // %s\n", updateActiveStr(iter.first, iter.second.first).c_str(), iter.second.second.c_str());
  }
#ifdef PERF
  for (int id : activateId) {
    fprintf(fp, "if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\n activator[%d][%d] ++;\n",
                id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
  }
  if (inStep) fprintf(fp, "isActivateValid = true;\n");
#endif
  if (!node->fullyUpdated) fprintf(fp, "}\n");
}

void graph::genNodeInsts(FILE* fp, Node* node) {
  std::string oldnode;
  if (node->insts.size()) {
    /* local variables */
    if (node->status == VALID_NODE && node->type == NODE_OTHERS && !node->needActivate() && node->width <= BASIC_WIDTH && !node->isArray()) {
      fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), node->name.c_str());
    }
    if (node->isArray() && !node->fullyUpdated) fprintf(fp, "bool %s = false;\n", ASSIGN_INDI(node).c_str());
    std::vector<std::string> newInsts(node->insts);
    /* save oldVal */
    if (node->needActivate() && !node->isArray()) {
      oldnode = saveOldVal(fp, node);
      for (size_t i = 0; i < newInsts.size(); i ++) {
        std::string inst = newInsts[i];
        size_t start_pos = 0;
        if (node->width <= BASIC_WIDTH) {
          std::string basicSet = node->name + " = ";
          std::string replaceStr = newName(node) + " = ";
          while((start_pos = inst.find(basicSet, start_pos)) != std::string::npos) {
            inst.replace(start_pos, basicSet.length(), replaceStr);
            start_pos += replaceStr.length();
          }
        } else {
          std::string mpzSet = "(" + node->name + ",";
          std::string replaceStr = "(newValMpz,";
          while((start_pos = inst.find(mpzSet, start_pos)) != std::string::npos) {
            inst.replace(start_pos, mpzSet.length(), replaceStr);
            start_pos += replaceStr.length();
          }
        }
        newInsts[i] = inst;
      }
    }
    /* display all insts */
    std::string indiStr = node->fullyUpdated || !node->isArray() ? "" : format("\n%s = true;\n", ASSIGN_INDI(node).c_str());
    for (std::string inst : newInsts) {
      fprintf(fp, "%s\n", strReplace(inst, ASSIGN_LABLE, indiStr).c_str());
    }
  }
  if (!node->needActivate()) ;
  else if(!node->isArray()) activateNext(fp, node, node->nextActiveId, newName(node), true);
  else activateUncondNext(fp, node, node->nextActiveId, true);
}

void graph::genNodeStepStart(FILE* fp, SuperNode* node) {
  nodeNum ++;
  if(node->superType == SUPER_SAVE_REG) {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
    fprintf(fp, "saveDiffRegs();\n");
#endif
  } else {
    int id;
    uint64_t mask;
    std::tie(id, mask) = clearIdxMask(node->cppId);
  #ifdef PERF
    fprintf(fp, "activeTimes[%d] ++;\n", node->cppId);
    fprintf(fp, "bool isActivateValid = false;\n");
  #endif
  }
}

void graph::nodeDisplay(FILE* fp, SuperNode* super) {
#ifdef EMU_LOG
  for (Node* member : super->member) {
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
      if (member->width > BASIC_WIDTH) {
        fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", nameIdx.c_str());
      } else if (member->width > 64) {
        fprintf(fp, "printf(\"%%lx|%%lx \", (uint64_t)(%s >> 64), (uint64_t(%s)));", nameIdx.c_str(), nameIdx.c_str());
      } else {
        fprintf(fp, "printf(\"%%lx \", (uint64_t(%s)));", nameIdx.c_str());
      }
      fprintf(fp, "\n%s", bracket.c_str());
      fprintf(fp, "printf(\"\\n\");\n");
    } else if (member->width > BASIC_WIDTH) {
      fprintf(fp, "printf(\"%%ld %d %s: \", cycles);\n", super->cppId, member->name.c_str());
      fprintf(fp, "mpz_out_str(stdout, 16, %s);\n", member->name.c_str());
      fprintf(fp, "printf(\"\\n\");\n");
    } else if (member->width > 64 && member->width <= BASIC_WIDTH) {
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
#endif
}

void graph::genNodeStepEnd(FILE* fp, SuperNode* node) {
#ifdef PERF
  fprintf(fp, "validActive[%d] += isActivateValid;\n", node->cppId);
#endif

  nodeDisplay(fp, node);
}

void graph::genActivate(FILE* fp) {
  int idx = 0;
  for (int i = 0; i < subStepNum; i ++) {
    fprintf(fp, "void S%s::subStep%d(){\n", name.c_str(), i);
    for (int i = 0; i < NODE_PER_SUBFUNC && idx < superId; i ++, idx ++) {
      int id;
      uint64_t mask;
      std::tie(id, mask) = setIdxMask(idx);
      if (idx % ACTIVE_WIDTH == 0) {
        if (i != 0) {
          fprintf(fp, "}\n");
        }
        fprintf(fp, "if(unlikely(activeFlags[%d] != 0)) {\n", id);
        fprintf(fp, "uint%d_t oldFlag = activeFlags[%d];\n", ACTIVE_WIDTH, id);
        fprintf(fp, "activeFlags[%d] = 0;\n", id);
      }
      fprintf(fp, "if(unlikely(oldFlag & 0x%lx)) { // id=%d\n", mask, idx);
      SuperNode* super = cppId2Super[idx];
      genNodeStepStart(fp, super);
      for (Node* n : super->member) {
        if (n->insts.size() == 0) continue;
        genNodeInsts(fp, n);
      }
      genNodeStepEnd(fp, super);
      fprintf(fp, "}\n");
    }
    fprintf(fp, "}\n");
    fprintf(fp, "}\n");
  }
}

void graph::genReset(FILE* fp, SuperNode* super, bool isUIntReset) {
  std::string resetName = super->resetNode->type == NODE_REG_SRC ? RESET_NAME(super->resetNode).c_str() : super->resetNode->name.c_str();
  fprintf(fp, "if(unlikely(%s)) {\n", resetName.c_str());
  std::set<int> allNext;
  for (size_t i = 0; i < super->member.size(); i ++) {
    Node* node = super->member[i];
    for (Node* next : node->next) {
      if (next->super->cppId >= 0) allNext.insert(next->super->cppId);
    }
    if (!node->regSplit) {
      if (node->getDst()->super->cppId >= 0) allNext.insert(node->getDst()->super->cppId);
    } else {
      if (node->getSrc()->regUpdate->super->cppId >= 0) allNext.insert(node->regUpdate->super->cppId);
    }
  }

  if (allNext.size() > 100) fprintf(fp, "activateAll();\n");
  else {
    std::map<uint64_t, std::pair<uint64_t, std::string>> bitMapInfo;
    activeSet2bitMap(allNext, bitMapInfo, -1);
    for (auto iter : bitMapInfo) {
      fprintf(fp, "%s // %s\n", updateActiveStr(iter.first, iter.second.first).c_str(), iter.second.second.c_str());
    }
  }
  for (size_t i = 0; i < super->member.size(); i ++) {
    for (std::string str : isUIntReset ? super->member[i]->resetInsts : super->member[i]->insts) {
      str = strReplace(str, ASSIGN_LABLE, "");
      str = strReplace(str, format("if(%s)", resetName.c_str()), "");
      if (super->resetNode->type == NODE_REG_SRC)
        fprintf(fp, "%s\n", strReplace(str, "(" + super->resetNode->name + ")", "(" + RESET_NAME(super->resetNode) + ")").c_str());
      else fprintf(fp, "%s\n", str.c_str());
    }
  }
  fprintf(fp, "}\n");
}

void graph::genResetAll(FILE* fp) {
  fprintf(fp, "void S%s::resetAll(){\n", name.c_str());
  for (SuperNode* super : sortedSuper) {
    if (super->superType == SUPER_ASYNC_RESET) {
      Assert(super->resetNode->isAsyncReset(), "invalid reset");
      genReset(fp, super, false);
    }
  }
  for (SuperNode* super : uintReset) {
    if (super->resetNode->status == CONSTANT_NODE) {
      Assert(mpz_sgn(super->resetNode->computeInfo->consVal) == 0, "reset %s is always true", super->resetNode->name.c_str());
      continue;
    }
    genReset(fp, super, true);
  }
  fprintf(fp, "}\n");
}

void graph::genMemWrite(FILE* fp) {
  /* update memory*/
  /* writer affects other nodes through reader, no need to activate in writer */
  fprintf(fp, "void S%s::writeMem(){\n", name.c_str());
  for (Node* mem : memory) {
    std::set<int> readerNextId;
    for (Node* port : mem->member) {
      if (port->type == NODE_READER) {
        if (port->get_member(READER_DATA)->super->cppId >= 0)
          readerNextId.insert(port->get_member(READER_DATA)->super->cppId);
      }
    }
    Assert(mem->rlatency <= 1 && mem->wlatency == 1, "rlatency %d wlatency %d in mem %s\n", mem->rlatency, mem->wlatency, mem->name.c_str());
    for (Node* port : mem->member) {
      if (port->type == NODE_WRITER) {
        valInfo* info_en = port->member[WRITER_EN]->computeInfo;
        if (info_en->status == VAL_CONSTANT && mpz_sgn(info_en->consVal) == 0) continue;
        if (port->member[WRITER_DATA]->isArray() != 0) {
          Node* writerData = port->member[WRITER_DATA];
          fprintf(fp, "if(unlikely(%s)) {\n", port->member[WRITER_EN]->computeInfo->valStr.c_str());
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
          fprintf(fp, "if(unlikely(%s)) {\n", cond.c_str());
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
        std::map<uint64_t, std::pair<uint64_t, std::string>> bitMapInfo;
        activeSet2bitMap(readerNextId, bitMapInfo, -1);
        for (auto iter : bitMapInfo) {
          fprintf(fp, "%s // %s\n", updateActiveStr(iter.first, iter.second.first).c_str(), iter.second.second.c_str());
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

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->isReset() && member->type == NODE_REG_SRC) {
        fprintf(fp, "%s = %s;\n", RESET_NAME(member).c_str(), member->name.c_str());
      }
    }
  }

  for (int i = 0; i < subStepNum; i ++) {
    fprintf(fp, "subStep%d();\n", i);
  }

  fprintf(fp, "writeMem();\n");
  fprintf(fp, "resetAll();\n");
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
    if (super->superType == SUPER_SAVE_REG || !super->instsEmpty()) {
      super->cppId = superId ++;
      cppId2Super[super->cppId] = super;
    }
  }
  activeFlagNum = (superId + ACTIVE_WIDTH - 1) / ACTIVE_WIDTH;
  subStepNum = (superId + NODE_PER_SUBFUNC - 1) / NODE_PER_SUBFUNC;
  updateRegNum = (regsrc.size() + NODE_PER_SUBFUNC - 1) / NODE_PER_SUBFUNC;

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
    if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
      for (Node* n : super->member) genNodeDef(header, n);
    }
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

  fprintf(header, "void writeMem();\n");
  fprintf(header, "void resetAll();\n");

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  fprintf(header, "void saveDiffRegs();\n");
#endif

  /* main evaluation loop (step)*/
  genActivate(src);
  genMemWrite(src);
  genResetAll(src);
  saveDiffRegs(src);
  genStep(src);
  
  genHeaderEnd(header);
  genSrcEnd(src);

  fclose(header);
  fclose(src);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFile);
#endif
  printf("[cppEmitter] define %ld nodes\n", definedNode.size());
}