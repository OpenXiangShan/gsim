/**
 * @file topoProjExport.cpp
 * @brief Export the (optionally flattened) gsim node graph and the coarsen /
 *        DP block assignments in the topo-partition-proj JSONL formats
 *        (wolvrix.am-instruction-graph.v1 / wolvrix.am-block-assignment.v1)
 *        so the topo-partition-proj harness can consume gsim's graph and
 *        gsim's own partition decisions.
 *
 * Mapping (gsim -> topo-proj):
 * - instruction      := a Node that is a VALID_NODE member of sortedSuper
 *                       (exactly what graphPartition sees). ids are dense and
 *                       assigned in sortedSuper (topological) order.
 * - op               := opType of the node's single compute enode after
 *                       flatten (or tree root). Synthetic op codes:
 *                       60 REF (leaf-rooted connect), 61 CONST_INT,
 *                       62 INPUT, 63 REG_UPDATE (reg_src update),
 *                       64 NONE (no tree, e.g. undriven/ext input stub).
 * - def_use edge     := a ref-enode reference T -> N (var = T).
 * - external_read    := reads of interface inputs (NODE_INP), register state
 *                       (NODE_REG_SRC) and memory contents (OP_READ_MEM).
 *                       Memories are not nodes; they get synthetic variable
 *                       ids appended after node ids. reg_src is exported as
 *                       an isolated node (no value edges) so the graph stays
 *                       a DAG; the reg_dst -> reg_src update is NOT exported
 *                       (it is a cycle boundary, not a value edge).
 * - order edge       := reg reset dep edges (depPrev/depNext, includes
 *                       reg_src -> async-reset-condition and condition ->
 *                       reg_dst) and memory reader -> writer order.
 * - state_write      := NODE_REG_DST / NODE_WRITER / NODE_READWRITER.
 *
 * The assignment export additionally computes the production-side scoreboard
 * (dag_edges / compute_compute_value_pairs / incoming_copy_cost) under the
 * topo-partition-proj 口径 so exp/tools/reconcile_baseline.py can reconcile.
 */

#include "common.h"
#include <map>
#include <set>

#define TOPO_PROJ_OP_REF 60
#define TOPO_PROJ_OP_CONST 61
#define TOPO_PROJ_OP_INPUT 62
#define TOPO_PROJ_OP_REG_UPDATE 63
#define TOPO_PROJ_OP_NONE 64

static const char* topoProjOpName(int op) {
  switch (op) {
    case TOPO_PROJ_OP_REF: return "REF";
    case TOPO_PROJ_OP_CONST: return "CONST_INT";
    case TOPO_PROJ_OP_INPUT: return "INPUT";
    case TOPO_PROJ_OP_REG_UPDATE: return "REG_UPDATE";
    case TOPO_PROJ_OP_NONE: return "NONE";
    default: break;
  }
  static const char* opNames[] = {
    "OP_EMPTY", "OP_MUX", "OP_ADD", "OP_SUB", "OP_MUL", "OP_DIV", "OP_REM",
    "OP_LT", "OP_LEQ", "OP_GT", "OP_GEQ", "OP_EQ", "OP_NEQ", "OP_DSHL",
    "OP_DSHR", "OP_AND", "OP_OR", "OP_XOR", "OP_CAT", "OP_ASUINT",
    "OP_ASSINT", "OP_ASCLOCK", "OP_ASASYNCRESET", "OP_CVT", "OP_NEG",
    "OP_NOT", "OP_ANDR", "OP_ORR", "OP_XORR", "OP_PAD", "OP_SHL", "OP_SHR",
    "OP_HEAD", "OP_TAIL", "OP_BITS", "OP_BITS_NOSHIFT", "OP_INDEX_INT",
    "OP_INDEX", "OP_WHEN", "OP_PRINTF", "OP_ASSERT", "OP_EXIT", "OP_INT",
    "OP_GROUP", "OP_READ_MEM", "OP_WRITE_MEM", "OP_INFER_MEM", "OP_INVALID",
    "OP_RESET", "OP_SEXT", "OP_EXT_FUNC", "OP_STMT_SEQ", "OP_STMT_WHEN",
    "OP_STMT_NODE"
  };
  if (op >= 0 && op < (int)(sizeof(opNames) / sizeof(opNames[0]))) return opNames[op];
  return "OP_UNKNOWN";
}

static int topoProjNodeOp(Node* node) {
  switch (node->type) {
    case NODE_INP: return TOPO_PROJ_OP_INPUT;
    case NODE_REG_SRC: return TOPO_PROJ_OP_REG_UPDATE;
    case NODE_READER: return OP_READ_MEM;
    case NODE_WRITER: return OP_WRITE_MEM;
    case NODE_READWRITER: return OP_READ_MEM;
    default: break;
  }
  for (ExpTree* tree : node->assignTree) {
    ENode* root = tree ? tree->getRoot() : nullptr;
    if (!root) continue;
    if (root->getNode()) return TOPO_PROJ_OP_REF;
    if (root->opType == OP_INT) return TOPO_PROJ_OP_CONST;
    if (root->opType == OP_EMPTY) continue;
    return root->opType;
  }
  return TOPO_PROJ_OP_NONE;
}

static bool topoProjStateWrite(Node* node) {
  return node->type == NODE_REG_DST || node->type == NODE_WRITER || node->type == NODE_READWRITER;
}

/* collect ref-enode targets of one enode subtree; refs are opaque except
   their OP_INDEX / OP_INDEX_INT children (same rule as updateConnect) */
static void topoProjCollectRefs(ENode* enode, std::set<Node*>& refs) {
  if (!enode) return;
  std::vector<ENode*> work(1, enode);
  while (!work.empty()) {
    ENode* cur = work.back();
    work.pop_back();
    if (cur->getNode()) {
      refs.insert(cur->getNode());
    }
    for (ENode* child : cur->child) {
      if (child) work.push_back(child);
    }
  }
}

void graph::exportTopoProjGraph(const std::string& path) {
  FILE* fp = fopen(path.c_str(), "w");
  Assert(fp, "exportTopoProjGraph: cannot open %s", path.c_str());

  topoProjNodeId.clear();
  topoProjMemVar.clear();
  topoProjNodes.clear();
  topoProjMems.clear();
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->status != VALID_NODE) continue;
      topoProjNodeId[node] = (int)topoProjNodes.size();
      topoProjNodes.push_back(node);
    }
  }
  const size_t nodeCount = topoProjNodes.size();

  /* buffers for header counts */
  size_t defUseEdges = 0, externalReads = 0, orderEdges = 0;
  std::set<uint64_t> orderSeen;

  /* first pass: discover memory variables (readers/writers/readwriters) */
  for (Node* node : topoProjNodes) {
    if (node->type != NODE_READER && node->type != NODE_WRITER && node->type != NODE_READWRITER) continue;
    for (ExpTree* tree : node->assignTree) {
      std::vector<ENode*> work;
      if (tree && tree->getRoot()) work.push_back(tree->getRoot());
      while (!work.empty()) {
        ENode* cur = work.back();
        work.pop_back();
        if (cur->memoryNode && topoProjMemVar.find(cur->memoryNode) == topoProjMemVar.end()) {
          topoProjMemVar[cur->memoryNode] = (int)(nodeCount + topoProjMems.size());
          topoProjMems.push_back(cur->memoryNode);
        }
        for (ENode* child : cur->child) {
          if (child) work.push_back(child);
        }
      }
    }
  }

  fprintf(fp, "{\"record\":\"header\",\"format\":\"wolvrix.am-instruction-graph.v1\",");
  /* header counts are patched after the body is written (fixed-width fields) */
  long headerEnd = ftell(fp);
  fprintf(fp, "\"instructions\":%20ld,\"variables\":%20ld,\"atoms\":%20ld,\"comb_loop_atoms\":0,"
              "\"def_use_edges\":%20ld,\"external_reads\":%20ld,\"order_edges\":%20ld,"
              "\"source\":\"gsim\",\"flatten_nodes\":%d,\"synthetic_ops\":{\"60\":\"REF\",\"61\":\"CONST_INT\",\"62\":\"INPUT\",\"63\":\"REG_UPDATE\",\"64\":\"NONE\"}}\n",
          (long)nodeCount, (long)(nodeCount + topoProjMems.size()), (long)nodeCount,
          0L, 0L, 0L, globalConfig.FlattenNodes ? 1 : 0);

  for (Node* node : topoProjNodes) {
    int id = topoProjNodeId[node];
    int op = topoProjNodeOp(node);
    fprintf(fp, "{\"record\":\"node\",\"id\":%d,\"op\":%d,\"opcode\":\"%s\",\"width\":%d,"
                "\"state_write\":%s,\"atom\":%d,\"comb_loop_atom\":false,\"gsim_type\":%d,\"name\":\"%s\"}\n",
            id, op, topoProjOpName(op), MAX(node->width, 0),
            topoProjStateWrite(node) ? "true" : "false", id, (int)node->type, node->name.c_str());
  }

  for (Node* node : topoProjNodes) {
    int dst = topoProjNodeId[node];
    if (node->type == NODE_REG_SRC) {
      /* reg_src is an isolated state node: its value read is exported as
         external_read at the consumers, and the reg_dst -> reg_src update
         (a cycle boundary) is intentionally not an edge */
      continue;
    }
    std::set<Node*> refs;
    for (ExpTree* tree : node->assignTree) {
      if (!tree) continue;
      topoProjCollectRefs(tree->getRoot(), refs);
      ENode* lval = tree->getlval();
      if (lval) {
        for (size_t i = 0; i < lval->getChildNum(); i ++) topoProjCollectRefs(lval->getChild(i), refs);
      }
    }
    if (node->type == NODE_SPECIAL && node->effectClock) topoProjCollectRefs(node->effectClock, refs);
    refs.erase(node);
    for (Node* ref : refs) {
      if (!topoProjNodeId.count(ref)) continue;
      if (ref->type == NODE_INP || ref->type == NODE_REG_SRC) {
        fprintf(fp, "{\"record\":\"edge\",\"kind\":\"external_read\",\"dst\":%d,\"var\":%d,\"width\":%d}\n",
                dst, topoProjNodeId[ref], MAX(ref->width, 0));
        externalReads ++;
      } else {
        int src = topoProjNodeId[ref];
        fprintf(fp, "{\"record\":\"edge\",\"kind\":\"def_use\",\"src\":%d,\"dst\":%d,\"var\":%d,\"width\":%d}\n",
                src, dst, src, MAX(ref->width, 0));
        defUseEdges ++;
      }
    }
    /* memory reads become external reads of the memory variable */
    if (node->type == NODE_READER || node->type == NODE_READWRITER) {
      for (ExpTree* tree : node->assignTree) {
        std::vector<ENode*> work;
        if (tree && tree->getRoot()) work.push_back(tree->getRoot());
        while (!work.empty()) {
          ENode* cur = work.back();
          work.pop_back();
          if (cur->opType == OP_READ_MEM && cur->memoryNode && topoProjMemVar.count(cur->memoryNode)) {
            fprintf(fp, "{\"record\":\"edge\",\"kind\":\"external_read\",\"dst\":%d,\"var\":%d,\"width\":%d}\n",
                    dst, topoProjMemVar[cur->memoryNode], MAX(node->width, 0));
            externalReads ++;
          }
          for (ENode* child : cur->child) {
            if (child) work.push_back(child);
          }
        }
      }
    }
  }

  /* memory reader -> writer order edges */
  for (Node* mem : topoProjMems) {
    std::vector<int> readers, writers;
    for (Node* port : mem->member) {
      if (!topoProjNodeId.count(port)) continue;
      if (port->type == NODE_READER || port->type == NODE_READWRITER) readers.push_back(topoProjNodeId[port]);
      if (port->type == NODE_WRITER || port->type == NODE_READWRITER) writers.push_back(topoProjNodeId[port]);
    }
    for (int r : readers) {
      for (int w : writers) {
        if (r == w) continue;
        uint64_t key = ((uint64_t)r << 32) | (uint32_t)w;
        if (orderSeen.insert(key).second) {
          fprintf(fp, "{\"record\":\"edge\",\"kind\":\"order\",\"src\":%d,\"dst\":%d}\n", r, w);
          orderEdges ++;
        }
      }
    }
  }

  /* reg reset dep edges (depPrev/depNext built by connectDep) */
  for (Node* node : topoProjNodes) {
    int dst = topoProjNodeId[node];
    for (Node* dep : node->depPrev) {
      if (!topoProjNodeId.count(dep)) continue;
      int src = topoProjNodeId[dep];
      uint64_t key = ((uint64_t)src << 32) | (uint32_t)dst;
      if (orderSeen.insert(key).second) {
        fprintf(fp, "{\"record\":\"edge\",\"kind\":\"order\",\"src\":%d,\"dst\":%d}\n", src, dst);
        orderEdges ++;
      }
    }
  }

  /* rewrite the header with the real edge counts (same fixed width) */
  long curEnd = ftell(fp);
  fseek(fp, headerEnd, SEEK_SET);
  fprintf(fp, "\"instructions\":%20ld,\"variables\":%20ld,\"atoms\":%20ld,\"comb_loop_atoms\":0,"
              "\"def_use_edges\":%20ld,\"external_reads\":%20ld,\"order_edges\":%20ld,",
          (long)nodeCount, (long)(nodeCount + topoProjMems.size()), (long)nodeCount,
          (long)defUseEdges, (long)externalReads, (long)orderEdges);
  fseek(fp, curEnd, SEEK_SET);
  fclose(fp);
  printf("[topoProj] graph: %ld nodes %ld memvars, %ld def_use + %ld external_read + %ld order edges -> %s\n",
         nodeCount, topoProjMems.size(), defUseEdges, externalReads, orderEdges, path.c_str());
}

void graph::exportTopoProjAssignment(const std::string& path, const char* stage) {
  Assert(!topoProjNodes.empty() || topoProjNodeId.empty(), "exportTopoProjAssignment: graph not exported first");
  FILE* fp = fopen(path.c_str(), "w");
  Assert(fp, "exportTopoProjAssignment: cannot open %s", path.c_str());

  /* dense block ids in sortedSuper order */
  std::map<SuperNode*, int> blockId;
  std::vector<SuperNode*> blocks;
  for (SuperNode* super : sortedSuper) {
    blockId[super] = (int)blocks.size();
    blocks.push_back(super);
  }

  /* scoreboard under the topo-partition-proj 口径 */
  std::vector<int> instrBlock(topoProjNodes.size(), -1);
  for (SuperNode* super : blocks) {
    for (Node* node : super->member) {
      auto it = topoProjNodeId.find(node);
      if (it != topoProjNodeId.end()) instrBlock[it->second] = blockId[super];
    }
  }

  /* re-walk the graph exactly like exportTopoProjGraph to score it */
  std::set<uint64_t> dagPairs;
  std::set<uint64_t> valuePairs;
  uint64_t incomingCopyCost = 0;
  auto scoreValue = [&](int var, int width, int src, int dst) {
    int srcBlock = src >= 0 ? instrBlock[src] : -1;
    int dstBlock = instrBlock[dst];
    if (src >= 0) {
      if (srcBlock == dstBlock) return;
      dagPairs.insert(((uint64_t)srcBlock << 32) | (uint32_t)dstBlock);
    }
    uint64_t pair = ((uint64_t)var << 32) | (uint32_t)dstBlock;
    if (valuePairs.insert(pair).second) incomingCopyCost += MAX(1, (width + 63) / 64);
  };
  for (Node* node : topoProjNodes) {
    int dst = topoProjNodeId[node];
    if (node->type == NODE_REG_SRC) continue;
    std::set<Node*> refs;
    for (ExpTree* tree : node->assignTree) {
      if (!tree) continue;
      topoProjCollectRefs(tree->getRoot(), refs);
      ENode* lval = tree->getlval();
      if (lval) {
        for (size_t i = 0; i < lval->getChildNum(); i ++) topoProjCollectRefs(lval->getChild(i), refs);
      }
    }
    if (node->type == NODE_SPECIAL && node->effectClock) topoProjCollectRefs(node->effectClock, refs);
    refs.erase(node);
    for (Node* ref : refs) {
      if (!topoProjNodeId.count(ref)) continue;
      if (ref->type == NODE_INP || ref->type == NODE_REG_SRC) scoreValue(topoProjNodeId[ref], MAX(ref->width, 0), -1, dst);
      else scoreValue(topoProjNodeId[ref], MAX(ref->width, 0), topoProjNodeId[ref], dst);
    }
    if (node->type == NODE_READER || node->type == NODE_READWRITER) {
      for (ExpTree* tree : node->assignTree) {
        std::vector<ENode*> work;
        if (tree && tree->getRoot()) work.push_back(tree->getRoot());
        while (!work.empty()) {
          ENode* cur = work.back();
          work.pop_back();
          if (cur->opType == OP_READ_MEM && cur->memoryNode && topoProjMemVar.count(cur->memoryNode)) {
            scoreValue(topoProjMemVar[cur->memoryNode], MAX(node->width, 0), -1, dst);
          }
          for (ENode* child : cur->child) {
            if (child) work.push_back(child);
          }
        }
      }
    }
  }

  fprintf(fp, "{\"record\":\"header\",\"format\":\"wolvrix.am-block-assignment.v1\",\"stage\":\"%s\","
              "\"instructions\":%ld,\"blocks\":%ld,\"dag_edges\":%ld,"
              "\"compute_compute_value_pairs\":%ld,\"incoming_copy_cost\":%ld,"
              "\"source\":\"gsim\",\"flatten_nodes\":%d,\"supernode_max_size\":%d}\n",
          stage, (long)topoProjNodes.size(), (long)blocks.size(), (long)dagPairs.size(),
          (long)valuePairs.size(), incomingCopyCost,
          globalConfig.FlattenNodes ? 1 : 0, globalConfig.SuperNodeMaxSize);
  for (SuperNode* super : blocks) {
    const char* kind = "compute";
    fprintf(fp, "{\"record\":\"block\",\"id\":%d,\"kind\":\"%s\",\"size\":%ld,\"super_type\":%d}\n",
            blockId[super], kind, (long)super->member.size(), (int)super->superType);
  }
  for (size_t i = 0; i < topoProjNodes.size(); i ++) {
    Assert(instrBlock[i] >= 0, "exportTopoProjAssignment: node %s has no block", topoProjNodes[i]->name.c_str());
    fprintf(fp, "{\"record\":\"assign\",\"instr\":%ld,\"block\":%d}\n", (long)i, instrBlock[i]);
  }
  fclose(fp);
  printf("[topoProj] assignment(%s): %ld blocks, dag_edges %ld, value pairs %ld, cost %ld -> %s\n",
         stage, blocks.size(), dagPairs.size(), valuePairs.size(), incomingCopyCost, path.c_str());
}
