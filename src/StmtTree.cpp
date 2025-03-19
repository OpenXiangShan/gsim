#include "common.h"
#include <stack>

bool checkENodeEq(ENode* enode1, ENode* enode2);

bool checkCondEd(StmtNode* stmtNode, ENode* enode) {
  if (enode->opType == OP_WHEN && stmtNode->type == OP_STMT_WHEN) {
    if (stmtNode->child[0]->isENode && checkENodeEq(enode->getChild(0), stmtNode->getChild(0)->enode)) {
      return true;
    }
  }
  return false;
}
/* generate whens according to enode, and add stmtNode into child */
void StmtNode::generateWhenTree(ENode* lvalue, ENode* root) {
  Assert(root->opType == OP_WHEN, "require when");
  StmtNode* stmt = nullptr;
  std::stack<std::pair<StmtNode*, ENode*>> s;
  s.push(std::make_pair(stmt, root));
  while (!s.empty()) {
    StmtNode* parent;
    ENode* enode;
    std::tie(parent, enode) = s.top();
    s.pop();
    if (enode->opType == OP_WHEN) {
      StmtNode* stmtNode = new StmtNode(OP_STMT_WHEN);
      stmtNode->addChild(new StmtNode(enode->getChild(0)->dup()));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      if (enode->getChild(1)) {
        s.push(std::make_pair(stmtNode->getChild(1), enode->getChild(1)));
      }
      if (enode->getChild(2)) {
        s.push(std::make_pair(stmtNode->getChild(2), enode->getChild(2)));
      }
      if (parent) parent->addChild(stmtNode);
      else stmt = stmtNode;
    } else {
      parent->child.push_back(new StmtNode(new ExpTree(enode->dup(), lvalue->dup())));
    }
  }
  addChild(stmt);
}

void StmtTree::mergeExpTree(ExpTree* tree) {
  std::stack<std::pair<StmtNode*, ENode*>> s;
  s.push(std::make_pair(root, tree->getRoot()));
  while (!s.empty()) {
    StmtNode* stmtNode;
    ENode* enode;
    std::tie(stmtNode, enode) = s.top();
    s.pop();

    if (checkCondEd(stmtNode, enode)) { // directly merge when
      if (enode->getChild(1)) {
        s.push(std::make_pair(stmtNode->getChild(1), enode->getChild(1)));
      }
      if (enode->getChild(2)) {
        s.push(std::make_pair(stmtNode->getChild(2), enode->getChild(2)));
      }
    } else {
      if (enode->opType == OP_WHEN) {
        if (stmtNode->type == OP_STMT_SEQ) {
          bool match = false;
          for (size_t i = 0; i < stmtNode->child.size(); i ++) {
            if (stmtNode->getChild(i)->type == OP_STMT_WHEN && checkCondEd(stmtNode->getChild(i), enode)) {
              match = true;
              s.push(std::make_pair(stmtNode->getChild(i), enode));
              break;
            }
          }
          if (!match) {
            stmtNode->generateWhenTree(tree->getlval(), enode);
          }
        } else {
          TODO();
        }
      } else {
        Assert(stmtNode->type == OP_STMT_SEQ, "stmtNode %d is not seq", stmtNode->type);
        stmtNode->addChild(new StmtNode(new ExpTree(enode->dup(), tree->getlval()->dup())));
      }
    }
  }

}