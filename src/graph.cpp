#include "common.h"
#include <functional>

static constexpr const char* WAVEFORM_INVALID_LVALUE = "INVALID_STR";

static bool waveformShapeIsUnpadded(Node* node) {
  for (int dim : node->dimension) {
    if (upperPower2(dim) != dim) return false;
  }
  return true;
}

graph::WaveformNode* graph::findWaveformNode(Node* node) {
  auto it = waveformNodes.find(node);
  if (it != waveformNodes.end()) return &it->second;
  if (!node->isRetained()) return nullptr;
  return &waveformNodes.emplace(node, WaveformNode{WaveformNodeKind::Alias, nullptr, {}}).first->second;
}

bool graph::preserveWaveformAlias(Node* node, Node* source) {
  if (!globalConfig.TraceFst || !node || !source || node == source || node->width <= 0 ||
      node->width != source->width || node->dimension != source->dimension ||
      !waveformShapeIsUnpadded(node) || !waveformShapeIsUnpadded(source) ||
      node->type == NODE_MEMORY || source->type == NODE_MEMORY) return false;

  if (source->status == CONSTANT_NODE && source->computeInfo) {
    return preserveWaveformConstant(node, source->computeInfo->valStr);
  }

  WaveformNode* signal = findWaveformNode(node);
  if (!signal) return false;

  signal->kind = WaveformNodeKind::Alias;
  signal->source = source;
  signal->value.clear();
  source->retained = true;
  return true;
}

bool graph::preserveWaveformConstant(Node* node, const std::string& value) {
  if (!globalConfig.TraceFst || !node || node->width <= 0 || node->type == NODE_MEMORY ||
      !waveformShapeIsUnpadded(node)) return false;

  WaveformNode* signal = findWaveformNode(node);
  if (!signal) return false;

  signal->kind = WaveformNodeKind::Constant;
  signal->source = nullptr;
  signal->value = value;
  return true;
}

bool graph::preserveWaveformExpression(Node* node, std::set<Node*>& runtimeDependencies) {
  if (!globalConfig.TraceFst || !node || node->type != NODE_OTHERS || node->width <= 0 ||
      node->isArray() || node->assignTree.size() != 1) return false;

  std::set<Node*> visiting;
  std::function<ENode*(Node*)> expandNode;
  std::function<ENode*(ENode*)> expandTree;
  expandNode = [&](Node* current) -> ENode* {
    if (!current || current->width <= 0 || current->isArray()) return nullptr;
    if (current->status == CONSTANT_NODE && current->computeInfo) {
      return allocIntEnode(current->width,
                           mpz_get_str(nullptr, 10, current->computeInfo->consVal), current->sign);
    }
    if (current->type == NODE_INP || current->type == NODE_OUT ||
        current->type == NODE_EXT_IN || current->type == NODE_EXT_OUT) {
      runtimeDependencies.insert(current);
      return new ENode(current);
    }
    if (current->type != NODE_OTHERS || current->assignTree.size() != 1) return nullptr;
    if (!visiting.insert(current).second) return nullptr;
    ENode* expanded = expandTree(current->assignTree[0]->getRoot());
    visiting.erase(current);
    return expanded;
  };
  expandTree = [&](ENode* current) -> ENode* {
    if (!current || current->width <= 0) return nullptr;
    if (current->opType == OP_WHEN || current->opType == OP_RESET ||
        current->opType == OP_PRINTF || current->opType == OP_ASSERT ||
        current->opType == OP_EXIT || current->opType == OP_READ_MEM) return nullptr;
    if (current->getNode()) {
      Node* source = current->getNode();
      ENode* expanded = expandNode(source);
      if (!expanded) return nullptr;
      expanded->setWidth(current->width, current->sign);
      for (ENode* child : current->child) {
        ENode* expandedChild = expandTree(child);
        if (!expandedChild) return nullptr;
        expanded->addChild(expandedChild);
      }
      return expanded;
    }
    ENode* expanded = current->dup();
    for (size_t i = 0; i < current->child.size(); i ++) {
      ENode* child = expandTree(current->child[i]);
      if (current->child[i] && !child) return nullptr;
      expanded->setChild(i, child);
    }
    return expanded;
  };

  ENode* expression = expandTree(node->assignTree[0]->getRoot());
  if (!expression) return false;
  valInfo* info = expression->compute(node, WAVEFORM_INVALID_LVALUE, true);
  if (!info || info->status == VAL_FINISH || info->status == VAL_INVALID || info->valStr.empty()) return false;
  std::vector<ENode*> pending{expression};
  while (!pending.empty()) {
    ENode* current = pending.back();
    pending.pop_back();
    if (!current->computeInfo || current->computeInfo->valStr.empty() ||
        (current->computeInfo->status != VAL_VALID && current->computeInfo->status != VAL_CONSTANT)) return false;
    for (ENode* child : current->child) {
      if (child) pending.push_back(child);
    }
  }

  WaveformNode* signal = findWaveformNode(node);
  if (!signal) return false;

  signal->kind = WaveformNodeKind::Expression;
  signal->source = nullptr;
  signal->value = info->valStr;
  return true;
}

void graph::reconnectAll() {
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->clear_relation();
    }
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->updateConnect();
    }
  }
  connectDep();
  reconnectSuper();
}

void graph::connectDep() {
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->type == NODE_REG_SRC) member->updateDep();
    }
  }
}

void graph::reconnectSuper() {
  for (SuperNode* super : sortedSuper) {
    super->clear_relation();
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) member->constructSuperConnect();
  }
}

void graph::removeNodesNoConnect(NodeStatus status) {
  if (globalConfig.LogLevel > 1) {
    for (SuperNode* super : sortedSuper) {
      for (Node* n : super->member) {
        if (n->status == status) {
          fprintf(stderr, "[RemoveNodes] remove status=%d name=%s type=%d super=%d\n",
                  status, n->name.c_str(), n->type, super->id);
        }
      }
    }
  }
  for (SuperNode* super : sortedSuper) {
    super->member.erase(
      std::remove_if(super->member.begin(), super->member.end(), [status](const Node* n){
        return n->status == status && (!globalConfig.TraceFst || !n->isRetained());
      }),
      super->member.end()
    );
  }
  removeEmptySuper();
}

size_t graph::countNodes() {
  size_t ret = 0;
  for (SuperNode* super : sortedSuper) ret += super->member.size();
  return ret;
}

void graph::removeNodes(NodeStatus status) {
  removeNodesNoConnect(status);
  removeEmptySuper();
  reconnectSuper();
}

void graph::removeEmptySuper() {
  sortedSuper.erase(
    std::remove_if(sortedSuper.begin(), sortedSuper.end(), [](const SuperNode* super) {return (super->superType == SUPER_VALID || super->superType == SUPER_UPDATE_REG) && super->member.size() == 0; }),
    sortedSuper.end()
  );
}

void graph::orderAllNodes() {
  int order = 1;
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    sortedSuper[i]->order = i;
    for (size_t j = 0; j < sortedSuper[i]->member.size(); j ++) {
      sortedSuper[i]->member[j]->orderInSuper = j;
      sortedSuper[i]->member[j]->order = order ++;
    }
  }
}
