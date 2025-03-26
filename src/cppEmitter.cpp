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
#define RESET_PER_FUNC 400

#define ENABLE_ACTIVATOR false

#ifdef DIFFTEST_PER_SIG
FILE* sigFile = nullptr;
#endif

#define RESET_NAME(node) (node->name + "$RESET")
#define emitFuncDecl(...) __emitSrc(true, true, NULL, __VA_ARGS__)
#define emitBodyLock(...) __emitSrc(false, false, NULL, __VA_ARGS__)

#define ActiveType std::tuple<uint64_t, std::string, int>
#define ACTIVE_MASK(active) std::get<0>(active)
#define ACTIVE_COMMENT(active) std::get<1>(active)
#define ACTIVE_UNIQUE(active) std::get<2>(active)

static int superId = 0;
static int activeFlagNum = 0;
static std::set<Node*> definedNode;
static std::map<int, SuperNode*> cppId2Super;
static std::set<int> alwaysActive;
static std::vector<std::string> resetFuncs;
extern int maxConcatNum;
bool nameExist(std::string str);
static int resetFuncNum = 0;

static bool isAlwaysActive(int cppId) {
  return alwaysActive.find(cppId) != alwaysActive.end();
}

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

ActiveType activeSet2bitMap(std::set<int>& activeId, std::map<uint64_t, ActiveType>& bitMapInfo, int curId) {
  uint64_t ret = 0;
  std::string comment = "";
  int uniqueIdx = 0;
  for (int id : activeId) {
    if (isAlwaysActive(id)) continue;
    int bitMapId;
    uint64_t bitMapMask;
    std::tie(bitMapId, bitMapMask) = setIdxMask(id);
    int num = 64 / ACTIVE_WIDTH;
    if (curId >= 0 && id > curId && bitMapId == curId / ACTIVE_WIDTH) {
      if (ret == 0) uniqueIdx = id % ACTIVE_WIDTH;
      else uniqueIdx = -1;
      ret |= bitMapMask;
      comment += std::to_string(id) + " ";
    } else {
      int beg = bitMapId - bitMapId % num;
      int end = beg + num;
      int findType = 0;
      uint64_t newMask = bitMapMask << ((bitMapId - beg) * ACTIVE_WIDTH);
      std::string newComment = std::to_string(id);
      if (bitMapInfo.find(bitMapId) != bitMapInfo.end()) {
        ACTIVE_MASK(bitMapInfo[bitMapId]) |= bitMapMask;
        ACTIVE_COMMENT(bitMapInfo[bitMapId]) += " " + std::to_string(id);
        ACTIVE_UNIQUE(bitMapInfo[bitMapId]) = -1;
        findType = 1; // no nothing
      } else {
        for (int newId = beg; newId < end; newId ++) {
          if (bitMapInfo.find(newId) != bitMapInfo.end()) {
            newMask |= ACTIVE_MASK(bitMapInfo[newId]) << ((newId - beg) * ACTIVE_WIDTH);
            newComment += " " + ACTIVE_COMMENT(bitMapInfo[newId]);
            findType = 2;  // find to merge
            bitMapInfo.erase(newId);
          }
        }
      }
      if (findType == 0) bitMapInfo[bitMapId] = std::make_tuple(bitMapMask, std::to_string(id), id % ACTIVE_WIDTH);
      else if (findType == 2) bitMapInfo[beg] = std::make_tuple(newMask, newComment, -1);
    }
  }
  return std::make_tuple(ret, comment, uniqueIdx);
}

std::string updateActiveStr(int idx, uint64_t mask) {
  if (mask <= MAX_U8) return format("activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U16) return format("*(uint16_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U32) return format("*(uint32_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  return format("*(uint64_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
}

std::string updateActiveStr(int idx, uint64_t mask, std::string& cond, int uniqueId) {
  auto activeFlags = std::string("activeFlags[") + std::to_string(idx) + std::string("]");

  if (mask <= MAX_U8) {
    if (uniqueId >= 0) return format("%s |= %s << %d;", activeFlags.c_str(), cond.c_str(), uniqueId);
    else return format("%s |= -(uint8_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  }
  if (mask <= MAX_U16)
    return format("*(uint16_t*)&%s |= -(uint16_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  if (mask <= MAX_U32)
    return format("*(uint32_t*)&%s |= -(uint32_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  return format("*(uint64_t*)&%s |= -(uint64_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
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

void graph::genNodeInit(Node* node, int mode) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_UPDATE || node->status != VALID_NODE) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  for (std::string inst : node->initInsts) {
    std::stringstream ss(inst);
    std::string s;
    while (getline(ss, s, '\n')) {
      bool init0 = (s.find("= 0x0;") != std::string::npos);
      bool emit = ((mode == 0) && init0) || ((mode == 1) && !init0) || (mode == 2);
      if (emit) { emitBodyLock("  %s\n", strReplace(s, ASSIGN_LABLE, "").c_str()); }
    }
  }
}

FILE* graph::genHeaderStart() {
  FILE* header = std::fopen((globalConfig.OutputDir + "/" + name + ".h").c_str(), "w");

  fprintf(header, "#ifndef %s_H\n#define %s_H\n", name.c_str(), name.c_str());
  /* include all libs */
  includeLib(header, "iostream", true);
  includeLib(header, "vector", true);
  includeLib(header, "assert.h", true);
  includeLib(header, "stdlib.h", true);
  includeLib(header, "cstdint", true);
  includeLib(header, "ctime", true);
  includeLib(header, "iomanip", true);
  includeLib(header, "cstring", true);
  includeLib(header, "map", true);
  includeLib(header, "cstdarg", true);
  newLine(header);

  fprintf(header, "\n// User configuration\n");
  fprintf(header, "//#define ENABLE_LOG\n");
  fprintf(header, "//#define RANDOMIZE_INIT\n");

  fprintf(header, "\n#define Assert(cond, ...) do {"
                     "if (!(cond)) {"
                       "fprintf(stderr, \"\\33[1;31m\");"
                       "fprintf(stderr, __VA_ARGS__);"
                       "fprintf(stderr, \"\\33[0m\\n\");"
                       "assert(cond);"
                     "}"
                   "} while (0)\n");

  fprintf(header, "#ifndef __BITINT_MAXWIDTH__ // defined by clang\n");
  fprintf(header, "#error Please compile with clang 16 or above\n");
  fprintf(header, "#endif\n");

  fprintf(header, "#define likely(x) __builtin_expect(!!(x), 1)\n");
  fprintf(header, "#define unlikely(x) __builtin_expect(!!(x), 0)\n");
  fprintf(header, "void gprintf(const char *fmt, ...);\n");

  for (int num = 2; num <= maxConcatNum; num ++) {
    std::string param;
    for (int i = num; i > 0; i --) param += format(i == num ? "_%d" : ", _%d", i);
    std::string value;
    std::string type = widthUType(num * 64);
    for (int i = num; i > 0; i --) {
      value += format(i == num ? "((%s)_%d << %d) " : "| ((%s)_%d << %d)", type.c_str(), i, (i-1) * 64);
    }
    fprintf(header, "#define UINT_CONCAT%d(%s) (%s)\n", num, param.c_str(), value.c_str());
  }
  for (std::string str : extDecl) fprintf(header, "%s\n", str.c_str());
  newLine(header);
  return header;
}

void graph::genInterfaceInput(Node* input) {
  /* set by string */
  emitFuncDecl("void S%s::set_%s(%s val) {\n", name.c_str(), input->name.c_str(), widthUType(input->width).c_str());
  emitBodyLock("  if (%s != val) { \n", input->name.c_str());
  emitBodyLock("    %s = val;\n", input->name.c_str());
  for (std::string inst : input->insts) {
    emitBodyLock("    %s\n", inst.c_str());
  }
  /* update nodes in the same superNode */
  std::set<int> allNext;
  for (Node* next : input->next) {
    if (next->super->cppId >= 0) allNext.insert(next->super->cppId);
  }
  std::map<uint64_t, ActiveType> bitMapInfo;
  activeSet2bitMap(allNext, bitMapInfo, -1);
  for (auto iter : bitMapInfo) {
    emitBodyLock("    %s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
  }
  emitBodyLock("  }\n"
               "}\n");
}

void graph::genInterfaceOutput(Node* output) {
  /* TODO: fix constant output which is not exists in sortedsuper */
  if (std::find(sortedSuper.begin(), sortedSuper.end(), output->super) == sortedSuper.end()) return;
  emitFuncDecl("%s S%s::get_%s() {\n"
               "  return %s;\n"
               "}\n",
               widthUType(output->width).c_str(), name.c_str(),
               output->name.c_str(), output->computeInfo->valStr.c_str());
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
    fprintf(fp, "%s %s%s", widthUType(node->width).c_str(), node->name.c_str(), "$prev");

    if (node->isArray() && node->arrayEntryNum() != 1) {
      for (int dim : node->dimension) fprintf(fp, "[%d]", dim);
    }
    fprintf(fp, ";\n");
  }
  std::string verilatorName = name + "__DOT__" + node->name;
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
  std::string diffNodeName = node->type == NODE_REG_SRC ? (node->getSrc()->name + "$prev") : node->name;
  std::string originName = (node->type == NODE_REG_SRC ? node->getSrc()->name : node->name);
  if (node->type == NODE_MEMORY){

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
  for (auto iter : allNames)
    fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.second.c_str());
}
#endif

void graph::genNodeDef(FILE* fp, Node* node) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_UPDATE || node->status != VALID_NODE) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_OTHERS && !node->anyNextActive() && !node->isArray()) return;
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  genDiffSig(fp, node);
#endif
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  fprintf(fp, "%s %s", widthUType(node->width).c_str(), node->name.c_str());
  if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
  for (int dim : node->dimension) fprintf(fp, "[%d]", upperPower2(dim));
  fprintf(fp, "; // width = %d, lineno = %d\n", node->width, node->lineno);
  int w = node->width;
  bool needInitMask = (node->type != NODE_MEMORY) &&
    (((w < 64) && (w != 8 && w != 16 && w != 32 && w != 64)) || ((w > 64) && (w % 32 != 0)));
  if (needInitMask) {
    if (node->dimension.empty()) {
      emitBodyLock("  %s &= %s;\n", node->name.c_str(), bitMask(w).c_str());
    } else {
      int dims = node->dimension.size();
      for (int i = 0; i < dims; i ++) {
        emitBodyLock("  for (int i%d = 0; i%d < %d; i%d ++) {\n", i, i, node->dimension[i], i);
      }
      emitBodyLock("  %s", node->name.c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock("[i%d]", i); }
      emitBodyLock(" &= %s;\n", bitMask(w).c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock("}\n"); }
    }
  }

  /* save reset registers */
  if (node->isReset() && node->type == NODE_REG_SRC) {
    Assert(!node->isArray() && node->width <= BASIC_WIDTH, "%s is treated as reset (isArray: %d width: %d)", node->name.c_str(), node->isArray(), node->width);
    fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), RESET_NAME(node).c_str());
    if (needInitMask) {
      emitBodyLock("  %s = %s & %s;\n", RESET_NAME(node).c_str(), RESET_NAME(node).c_str(), bitMask(w).c_str());
    }
  }
}

std::string graph::saveOldVal(Node* node) {
  std::string ret;
  if (node->isArray()) return ret;
    /* save oldVal */
  if (node->fullyUpdated) {
    emitBodyLock("%s %s;\n", widthUType(node->width).c_str(), newBasic(node).c_str());
    ret = newBasic(node);
  } else {
    emitBodyLock("%s %s = %s;\n", widthUType(node->width).c_str(), newBasic(node).c_str(), node->name.c_str());
    ret = newBasic(node);
  }
  return ret;
}

void graph::activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName) {
  std::string nodeName = node->name;
  auto condName = std::string("cond_") + nodeName;
  bool opt{false};

  if (node->isArray() && node->arrayEntryNum() == 1) nodeName += strRepeat("[0]", node->dimension.size());
  std::map<uint64_t, ActiveType> bitMapInfo;
  ActiveType curMask;
  if (node->isAsyncReset()) {
    emitBodyLock("if (%s || (%s != %s)) {\n", oldName.c_str(), nodeName.c_str(), oldName.c_str());
  } else {
    curMask = activeSet2bitMap(nextNodeId, bitMapInfo, node->super->cppId);
    opt = ((ACTIVE_MASK(curMask) != 0) + bitMapInfo.size()) <= 3;
    if (opt) {
      if (node->width == 1) emitBodyLock("bool %s = %s ^ %s;\n", condName.c_str(), nodeName.c_str(), oldName.c_str());
      else emitBodyLock("bool %s = %s != %s;\n", condName.c_str(), nodeName.c_str(), oldName.c_str());
    }
    else emitBodyLock("if (%s != %s) {\n", nodeName.c_str(), oldName.c_str());
  }
  if (inStep) {
    if (node->isReset() && node->type == NODE_REG_SRC) emitBodyLock("%s = %s;\n", RESET_NAME(node).c_str(), newName(node).c_str());
    emitBodyLock("%s = %s;\n", node->name.c_str(), newName(node).c_str());
  }
  if (node->isAsyncReset()) {
    Assert(!opt, "invalid opt");
    emitBodyLock("activateAll();\n");
    emitBodyLock("%s = -1;\n", flagName.c_str());
  } else {
    if (ACTIVE_MASK(curMask) != 0) {
      if (opt) emitBodyLock("%s |= -(uint%d_t)%s & 0x%lx; // %s\n", flagName.c_str(), ACTIVE_WIDTH, condName.c_str() ,ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
      else emitBodyLock("%s |= 0x%lx; // %s\n", flagName.c_str(), ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
    }
    for (auto iter : bitMapInfo) {
      auto str = opt ? updateActiveStr(iter.first, ACTIVE_MASK(iter.second), condName, ACTIVE_UNIQUE(iter.second)) : updateActiveStr(iter.first, ACTIVE_MASK(iter.second));
      emitBodyLock("%s // %s\n", str.c_str(), ACTIVE_COMMENT(iter.second).c_str());
    }
  #ifdef PERF
    #if ENABLE_ACTIVATOR
    for (int id : nextNodeId) {
      emitBodyLock("if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\nactivator[%d][%d] ++;\n",
                  id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
    }
    #endif
    if (inStep) emitBodyLock("isActivateValid = true;\n");
  #endif
  }
  if (!opt) emitBodyLock("}\n");
}

void graph::activateUncondNext(Node* node, std::set<int>activateId, bool inStep, std::string flagName) {
  if (!node->fullyUpdated) emitBodyLock("if (%s) {\n", ASSIGN_INDI(node).c_str());
  std::map<uint64_t, ActiveType> bitMapInfo;
  auto curMask = activeSet2bitMap(activateId, bitMapInfo, node->super->cppId);
  if (ACTIVE_MASK(curMask) != 0) emitBodyLock("%s |= 0x%lx; // %s\n", flagName.c_str(), ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
  for (auto iter : bitMapInfo) {
    emitBodyLock("%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
  }
#ifdef PERF
  #if ENABLE_ACTIVATOR
  for (int id : activateId) {
    emitBodyLock("if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\n activator[%d][%d] ++;\n",
                id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
  }
  #endif
  if (inStep) emitBodyLock("isActivateValid = true;\n");
#endif
  if (!node->fullyUpdated) emitBodyLock("}\n");
}

void graph::genNodeInsts(Node* node, std::string flagName) {
  std::string oldnode;
  if (node->insts.size()) {
    /* local variables */
    if (node->status == VALID_NODE && node->type == NODE_OTHERS && !node->anyNextActive() && !node->isArray()) {
      emitBodyLock("%s %s;\n", widthUType(node->width).c_str(), node->name.c_str());
    }
    if (node->isArray() && !node->fullyUpdated && node->anyNextActive()) emitBodyLock("bool %s = false;\n", ASSIGN_INDI(node).c_str());
    std::vector<std::string> newInsts(node->insts);
    /* save oldVal */
    if (node->needActivate() && !node->isArray()) {
      oldnode = saveOldVal(node);
      for (size_t i = 0; i < newInsts.size(); i ++) {
        std::string inst = newInsts[i];
        size_t start_pos = 0;
        std::string basicSet = node->name + " = ";
        std::string replaceStr = newName(node) + " = ";
        while((start_pos = inst.find(basicSet, start_pos)) != std::string::npos) {
          inst.replace(start_pos, basicSet.length(), replaceStr);
          start_pos += replaceStr.length();
        }
        newInsts[i] = inst;
      }
    }
    /* display all insts */
    std::string indiStr;
    if (!node->fullyUpdated && node->isArray() && node->anyNextActive()) {
      indiStr = format("\n%s = true;\n", ASSIGN_INDI(node).c_str());
    } else if (node->type == NODE_WRITER) { // activate all readers
      indiStr += "\n";
      std::set<int> allReaderId;
      for (Node* port : node->parent->member) {
        if (port->type == NODE_READER && (port->status == VALID_NODE || port->status == MERGED_NODE)) allReaderId.insert(port->super->cppId);
      }
      std::map<uint64_t, ActiveType> bitMapInfo;
      activeSet2bitMap(allReaderId, bitMapInfo, -1);
      for (auto iter : bitMapInfo) {
        indiStr += format("%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
      }
    }
    for (std::string inst : newInsts) {
      emitBodyLock("%s\n", strReplace(inst, ASSIGN_LABLE, indiStr).c_str());
    }
  }
  if (!node->needActivate()) ;
  else if(!node->isArray()) activateNext(node, node->nextNeedActivate, newName(node), true, flagName);
  else activateUncondNext(node, node->nextNeedActivate, true, flagName);
}

void graph::genNodeStepStart(SuperNode* node, uint64_t mask, int idx, std::string flagName) {
  nodeNum ++;
  if (!isAlwaysActive(node->cppId)) emitBodyLock("if(unlikely(%s & 0x%lx)) { // id=%d\n", flagName.c_str(), mask, idx);
  int id;
  uint64_t newMask;
  std::tie(id, newMask) = clearIdxMask(node->cppId);
#ifdef PERF
  emitBodyLock("activeTimes[%d] ++;\n", node->cppId);
  emitBodyLock("bool isActivateValid = false;\n");
#endif
}

void graph::nodeDisplay(Node* member) {
#define emit_display(varname, width) \
  do { \
    int n = ROUNDUP(width, 64) / 64; \
    std::string s = "printf(\"%%lx"; \
    for (int i = n - 2; i >= 0; i --) { \
      s += "|%%lx"; \
    } \
    s += "\", "; \
    for (n --; n >= 0; n --) { \
      s += format("(uint64_t)(%s >> %d)", varname, n * 64); \
      if (n != 0) s += ", "; \
    } \
    s += ");"; \
    emitBodyLock(s.c_str()); \
  } while (0)

  if (member->status != VALID_NODE) return;
  emitBodyLock("#ifdef ENABLE_LOG\n");
  emitBodyLock("if (cycles >= LOG_START && cycles <= LOG_END) {\n");
  emitBodyLock("printf(\"%%ld %d %s: \", cycles);\n", member->super->cppId, member->name.c_str());
  if (member->dimension.size() != 0) {
    std::string idxStr, bracket;
    for (size_t i = 0; i < member->dimension.size(); i ++) {
      emitBodyLock("for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
      idxStr += "[i" + std::to_string(i) + "]";
      bracket += "}\n";
    }
    std::string nameIdx = member->name + idxStr;
    emit_display(nameIdx.c_str(), member->width);
    emitBodyLock("printf(\" \");\n");
    emitBodyLock("\n%s", bracket.c_str());
  } else {
    if (member->anyNextActive() || member->type != NODE_SPECIAL) {
      emit_display(member->name.c_str(), member->width);
    }
  }
  emitBodyLock("printf(\"\\n\");\n");
  emitBodyLock("}\n");
  emitBodyLock("#endif\n");
}

void graph::genNodeStepEnd(SuperNode* node) {
#ifdef PERF
  emitBodyLock("validActive[%d] += isActivateValid;\n", node->cppId);
#endif

  if(!isAlwaysActive(node->cppId)) emitBodyLock("}\n");
}

int graph::genActivate() {
    emitFuncDecl("void S%s::subStep0() {\n", name.c_str());
    int nextSubStepIdx = 1;
    std::string nextFuncDef = format("void S%s::subStep%d()", name.c_str(), nextSubStepIdx);
    bool prevActiveWhole = false;
    for (int idx = 0; idx < superId; idx ++) {
      int id;
      uint64_t mask;
      std::tie(id, mask) = setIdxMask(idx);
      int offset = idx % ACTIVE_WIDTH;
      if (offset == 0) {
        if (prevActiveWhole) {
          emitBodyLock("}\n");
        }
        prevActiveWhole = true;
        for (int j = 0; j < ACTIVE_WIDTH && idx + j < superId; j ++) {
          if (isAlwaysActive(idx + j)) prevActiveWhole = false;
        }
        if (prevActiveWhole) {
          bool newFile = __emitSrc(true, false, nextFuncDef.c_str(), "if(unlikely(activeFlags[%d] != 0)) {\n", id);
          if (newFile) {
            nextFuncDef = format("void S%s::subStep%d()", name.c_str(), ++ nextSubStepIdx);
          }
          emitBodyLock("uint%d_t oldFlag = activeFlags[%d];\n", ACTIVE_WIDTH, id);
          emitBodyLock("activeFlags[%d] = 0;\n", id);
        }
      }
      SuperNode* super = cppId2Super[idx];
      std::string flagName = prevActiveWhole ? "oldFlag" : format("activeFlags[%d]", id);
      genNodeStepStart(super, mask, idx, flagName);
      if (super->superType == SUPER_EXTMOD) {
        /* save old EXT_OUT*/
        for (size_t i = 1; i < super->member.size(); i ++) {
          if (!super->member[i]->needActivate()) continue;
          Node* extOut = super->member[i];
          emitBodyLock("%s %s = %s;\n", widthUType(extOut->width).c_str(), oldName(extOut).c_str(), extOut->name.c_str());
        }
        genNodeInsts(super->member[0], flagName);
        for (size_t i = 1; i < super->member.size(); i ++) {
          if (!super->member[i]->needActivate()) continue;
          if (super->member[i]->isArray()) activateUncondNext(super->member[i], super->member[i]->nextActiveId, false, flagName);
          else activateNext(super->member[i], super->member[i]->nextActiveId, oldName(super->member[i]), false, flagName);
        }
      } else {
        for (Node* n : super->member) {
          if (n->insts.size() == 0) {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
            if (n->type == NODE_REG_SRC) {
              if (n->isArray()) {
                std::string idxStr, bracket, inst;
                for (int i = 0; i < n->dimension.size(); i ++) {
                  inst += format("for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, n->dimension[i], i);
                  idxStr += "[i" + std::to_string(i) + "]";
                  bracket += "}\n";
                }
                inst += format("%s$prev%s = %s%s;\n", n->name.c_str(), idxStr.c_str(), n->name.c_str(), idxStr.c_str());
                inst += bracket;
                n->insts.push_back(inst);
              } else n->insts.push_back(format("%s$prev = %s;\n", n->name.c_str(), n->name.c_str()));
            }
#endif
            continue;
          }
          genNodeInsts(n, flagName);
          nodeDisplay(n);
        }
      }
      genNodeStepEnd(super);
    }
    emitBodyLock("}\n");
    if (prevActiveWhole) emitBodyLock("}\n");

    return nextSubStepIdx - 1; // return the maxinum subStepIdx currently used
}

void graph::genReset(SuperNode* super, bool isUIntReset) {
  std::string resetName = super->resetNode->type == NODE_REG_SRC ? RESET_NAME(super->resetNode).c_str() : super->resetNode->name.c_str();
  emitBodyLock("if(unlikely(%s)) {\n", resetName.c_str());
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

  if (allNext.size() > 100) emitBodyLock("activateAll();\n");
  else {
    std::map<uint64_t, ActiveType> bitMapInfo;
    activeSet2bitMap(allNext, bitMapInfo, -1);
    for (auto iter : bitMapInfo) {
      emitBodyLock("%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
    }
  }
  int resetNum = (super->member.size() + RESET_PER_FUNC - 1) / RESET_PER_FUNC;
  size_t idx = 0;
  for (int i = 0; i < resetNum; i ++, resetFuncNum ++) {
    emitBodyLock("subReset%d();\n", resetFuncNum);
    std::string resetFuncStr = format("void S%s::subReset%d(){\n", name.c_str(), resetFuncNum);
    for (int j = 0; j < RESET_PER_FUNC && idx < super->member.size(); j ++, idx ++) {
      for (std::string str : isUIntReset ? super->member[idx]->resetInsts : super->member[idx]->insts) {
        str = strReplace(str, ASSIGN_LABLE, "");
        str = strReplace(str, format("if (%s)", resetName.c_str()), "");
        if (super->resetNode->type == NODE_REG_SRC)
          resetFuncStr += strReplace(str, "(" + super->resetNode->name + ")", "(" + RESET_NAME(super->resetNode) + ")") + "\n";
        else resetFuncStr += str + "\n";
      }
    }
    resetFuncStr += "}\n";
    resetFuncs.push_back(resetFuncStr);
  }
  emitBodyLock("}\n");
}

void graph::genResetAll() {
  emitFuncDecl("void S%s::resetAll(){\n", name.c_str());
  for (SuperNode* super : sortedSuper) {
    if (super->superType == SUPER_ASYNC_RESET) {
      Assert(super->resetNode->isAsyncReset(), "invalid reset");
      genReset(super, false);
    }
  }
  for (SuperNode* super : uintReset) {
    if (super->resetNode->status == CONSTANT_NODE) {
      Assert(mpz_sgn(super->resetNode->computeInfo->consVal) == 0, "reset %s is always true", super->resetNode->name.c_str());
      continue;
    }
    genReset(super, true);
  }
  emitBodyLock("}\n");
  for (std::string str : resetFuncs) emitBodyLock("%s", str.c_str());
}

void graph::saveDiffRegs() {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  emitFuncDecl("void S%s::saveDiffRegs(){\n", name.c_str());
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->type == NODE_REG_SRC && (!member->isArray() || member->arrayEntryNum() == 1) && member->status == VALID_NODE) {
        std::string memberName = member->name;
        if (member->isArray() && member->arrayEntryNum() == 1) {
          for (size_t i = 0; i < member->dimension.size(); i ++) memberName += "[0]";
        }
        emitBodyLock("%s = %s;\n", arrayPrevName(member->getSrc()->name).c_str(), memberName.c_str());
      } else if (member->type == NODE_REG_SRC && member->isArray() && member->status == VALID_NODE) {
        std::string idxStr, bracket;
        for (size_t i = 0; i < member->dimension.size(); i ++) {
          emitBodyLock("for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
          idxStr += "[i" + std::to_string(i) + "]";
          bracket += "}\n";
        }
        emitBodyLock("%s$prev%s = %s%s;\n", member->name.c_str(), idxStr.c_str(), member->name.c_str(), idxStr.c_str());
        emitBodyLock("%s", bracket.c_str());
      }
    }
  }
  emitBodyLock("}\n");
#endif
}

void graph::genStep(int subStepIdxMax) {
  emitFuncDecl("void S%s::step() {\n", name.c_str());

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->isReset() && member->type == NODE_REG_SRC) {
        emitBodyLock("%s = %s;\n", RESET_NAME(member).c_str(), member->name.c_str());
      }
    }
  }
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  emitBodyLock("saveDiffRegs();\n");
#endif
  for (int i = 0; i <= subStepIdxMax; i ++) {
    emitBodyLock("subStep%d();\n", i);
  }
  emitBodyLock("resetAll();\n");
  emitBodyLock("cycles ++;\n");
  emitBodyLock("}\n");
}

bool SuperNode::instsEmpty() {
  for (Node* n : member) {
#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
    if (n->insts.size() != 0 || n->type == NODE_REG_SRC) return false;
#else
    if (n->insts.size() != 0) return false;
#endif
  }
  return true;
}

bool graph::__emitSrc(bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...) {
  bool newFile = false;
  if (srcFp == NULL || (srcFileBytes > (globalConfig.cppMaxSizeKB * 1024) && canNewFile)) {
    if (srcFp != NULL) {
      if (!alreadyEndFunc) fprintf(srcFp, "}"); // the end of the current function
      fclose(srcFp);
    }
    srcFp = std::fopen(format("%s%d.cpp", (globalConfig.OutputDir + "/" + name).c_str(), srcFileIdx).c_str(), "w");
    srcFileIdx ++;
    assert(srcFp != NULL);
    srcFileBytes = fprintf(srcFp, "#include \"%s.h\"\n", name.c_str());
    if (nextFuncDef != NULL) {
      srcFileBytes += fprintf(srcFp, "%s {\n", nextFuncDef);
    }
    newFile = true;
  }
  va_list args;
  va_start(args, fmt);
  int bytes = vfprintf(srcFp, fmt, args);
  assert(bytes > 0);
  va_end(args);
  srcFileBytes += bytes;
  return newFile;
}

void graph::emitPrintf() {
  emitFuncDecl("void gprintf(const char *fmt, ...) {\n");
  emitBodyLock(
  "  va_list args;\n"
  "  va_start(args, fmt);\n"
  "  int fmt_idx = 0;\n"
  "  while (true) {\n"
  "    char c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case '%%': break;\n"
  "      case 0: return;\n"
  "      default: putchar(c); continue;\n"
  "    }\n"
  "\n"
  "    uint64_t lval = 0;\n"
  "    int bits = va_arg(args, uint32_t);\n"
  "    if      (bits <= 32) { lval = va_arg(args, uint32_t); }\n"
  "    else if (bits <= 64) { lval = va_arg(args, uint64_t); }\n"
  "    else                 { assert(0); }\n"
  "\n"
  "    c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case 'd': printf(\"%%ld\", lval); break;\n"
  "      case 'c': putchar(lval & 0xff); break;\n"
  "      case 'x': printf(\"%%lx\", lval); break;\n"
  "      default: assert(0);\n"
  "    }\n"
  "  }\n"
  "}\n"
  );
}

void graph::cppEmitter() {
  for (SuperNode* super : sortedSuper) {
    if (!super->instsEmpty() || super->superType == SUPER_EXTMOD) {
      super->cppId = superId ++;
      cppId2Super[super->cppId] = super;
      if (super->superType == SUPER_EXTMOD) {
        alwaysActive.insert(super->cppId);
      }
#if 0
      if (super->member.size() == 1 && super->member[0]->ops < 1) {
        alwaysActive.insert(super->cppId);
        printf("alwaysActive %d\n", super->cppId);
      }
#endif
    }
  }
  activeFlagNum = (superId + ACTIVE_WIDTH - 1) / ACTIVE_WIDTH;

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE) {
        member->updateActivate();
        member->updateNeedActivate(alwaysActive);
      }
    }
  }

  srcFp = NULL;
  srcFileIdx = 0;

  FILE* header = genHeaderStart();
#ifdef DIFFTEST_PER_SIG
  sigFile = fopen((globalConfig.OutputDir + "/" + name + "_sigs.txt").c_str(), "w");
#endif

  /* class start*/
  fprintf(header, "class S%s {\npublic:\n", name.c_str());
  fprintf(header, "uint64_t cycles;\n");
  fprintf(header, "uint64_t LOG_START, LOG_END;\n");
  fprintf(header, "uint%d_t activeFlags[%d];\n", ACTIVE_WIDTH, activeFlagNum); // or super.size() if id == idx
#ifdef PERF
  fprintf(header, "size_t activeTimes[%d];\n", superId);
#if ENABLE_ACTIVATOR
  fprintf(header, "std::map<int, int>activator[%d];\n", superId);
#endif
  fprintf(header, "size_t validActive[%d];\n", superId);
  fprintf(header, "size_t nodeNum[%d];\n", superId);
#endif
  emitPrintf();
  /* constrcutor */
  emitFuncDecl("S%s::S%s() {\n"
               "  cycles = 0;\n"
               "  LOG_START = 1;\n"
               "  LOG_END = 0;\n"
               "  init();\n"
               "}\n", name.c_str(), name.c_str());

  /* initialization */
  emitFuncDecl("void S%s::init() {\n", name.c_str());
  emitBodyLock("  activateAll();\n");
#ifdef PERF
  emitBodyLock("  for (int i = 0; i < %d; i ++) activeTimes[i] = 0;\n", superId);
  #if ENABLE_ACTIVATOR
  emitBodyLock("  for (int i = 0; i < %d; i ++) activator[i] = std::map<int, int>();\n", superId);
  #endif
  for (SuperNode* super : sortedSuper) {
    if (super->cppId >= 0) {
      size_t num = 0;
      for (Node* member : super->member) {
        if (member->anyNextActive()) num ++;
      }
      emitBodyLock("  nodeNum[%d] = %ld; // memberNum=%ld\n", super->cppId, num, super->member.size());
    }
  }
  emitBodyLock("  for (int i = 0; i < %d; i ++) validActive[i] = 0;\n", superId);
#endif

  emitBodyLock("#ifdef RANDOMIZE_INIT\n"
               "  srand((unsigned int)time(NULL));\n"
               "  for (uint32_t *p = &_var_start; p != &_var_end; p ++) {\n"
               "    *p = rand();\n"
               "  }\n"
               "// mask out the bits out of the width range\n");

  // header: node definition; src: node evaluation
  fprintf(header, "uint32_t _var_start;\n");
  for (SuperNode* super : sortedSuper) {
    // std::string insts;
    if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
      for (Node* n : super->member) genNodeDef(header, n);
    }
    if (super->superType == SUPER_EXTMOD) {
      for (size_t i = 1; i < super->member.size(); i ++) genNodeDef(header, super->member[i]);
    }
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDef(header, mem);
  fprintf(header, "uint32_t _var_end;\n");

  emitBodyLock("// initialize registers with reset value 0 to overwrite the rand() results\n" );
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID && super->superType != SUPER_ASYNC_RESET) continue;
    for (Node* member : super->member) {
      genNodeInit(member, 0);
    }
  }

  emitBodyLock("#else\n" // RANDOMIZE_INIT
               "  memset(&_var_start, 0, &_var_end - &_var_start);\n"
               "#endif\n");

  fprintf(header, "S%s();\n", name.c_str());
  fprintf(header, "void init();\n");

  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID && super->superType != SUPER_ASYNC_RESET) continue;
    for (Node* member : super->member) {
      genNodeInit(member, 1);
    }
  }

  emitBodyLock("}\n");

  /* activation all nodes for reset */
  fprintf(header, "void activateAll();\n");
  emitFuncDecl("void S%s::activateAll() {\n"
               "  memset(activeFlags, 0xff, sizeof(activeFlags));\n"
               "}\n", name.c_str());

   /* input/output interface */
  for (Node* node : input) {
    fprintf(header, "void set_%s(%s val);\n", node->name.c_str(), widthUType(node->width).c_str());
    genInterfaceInput(node);
  }
  for (Node* node : output) {
    fprintf(header, "%s get_%s();\n", widthUType(node->width).c_str(), node->name.c_str());
    genInterfaceOutput(node);
  }

  /* reset functions */
  fprintf(header, "void resetAll();\n");
  genResetAll();
  for (int i = 0; i < resetFuncNum; i ++) {
    fprintf(header, "void subReset%d();\n", i);
  }

  /* main evaluation loop (step) */
  int subStepIdxMax = genActivate();
  for (int i = 0; i <= subStepIdxMax; i ++) {
    fprintf(header, "void subStep%d();\n", i);
  }

  /* step wrapper */
  fprintf(header, "void step();\n");
  genStep(subStepIdxMax);

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
  fprintf(header, "void saveDiffRegs();\n");
#endif
  saveDiffRegs();

  /* end of file */
  fprintf(header, "};\n"
                  "#endif\n");
  fclose(header);
  fclose(srcFp);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFile);
#endif

  printf("[cppEmitter] define %ld nodes %d superNodes\n", definedNode.size(), superId);
  std::cout << "[cppEmitter] finish writing " << srcFileIdx << " cpp files to " + globalConfig.OutputDir + "/" << std::endl;
}
