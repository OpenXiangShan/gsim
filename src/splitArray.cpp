#include "common.h"
#include <stack>
#include <set>
#include <map>
static std::map<Node*, std::set<Node*>>prevArray;
static std::set<Node*> allArrays;

void graph::splitArray() {
  std::map<Node*, int> times;
  std::stack<Node*> s;
  // std::set<Node*>visited;
  for (SuperNode* super : supersrc) {
    for (Node* node : super->member) s.push(node);
  }

  while(!s.empty()) {
    Node* top = s.top();
    s.pop();
    if (top->isArray()) allArrays.insert(top);
    for (Node* next : top->next) {
      if (times.find(next) == times.end()) {
        // TODO: 如果构成环则不会被加入s中
        times[next] = 0;
      }
      times[next] ++;
      if (top->isArray()) prevArray[next].insert(top);
      prevArray[next].insert(prevArray[top].begin(), prevArray[top].end());
      if (times[next] == (int)next->prev.size()) s.push(next);
    }
  }
  for (auto iter : allArrays) {
    printf("-----%s----\n", iter->name.c_str());
    for (Node* node : prevArray[iter]) printf("  %s\n", node->name.c_str());
  }

  for (auto iter : allArrays) {
    if (prevArray[iter].find(iter) != prevArray[iter].end()) { // self loop, need to split
      iter->splitArray();
    }
  }

}

void Node::splitArray() {
  Assert(isArray(), "%s is not array", name.c_str());
  Assert(arrayMember.size() == 0, "%s is already splitted", name.c_str());
  int num = 1;
  for (size_t i = 0; i < dimension.size(); i ++) {
    num *= dimension[i];
  }
  /* allocate member nodes */
  for (int i = 0; i < num; i ++) {
    Node* member = new Node(NODE_OTHERS);
    member->arrayParent = this;
    member->name = arrayMemberName(i);
    member->constructSuperNode();
    arrayMember.push_back(member);
  }
  /* distribute expTrees in arrayVal to each member */
  for (ExpTree* tree : arrayVal) {
    int begin, end;
    std::tie(begin, end) = tree->getlval()->getIdx(this);
    Assert(begin >= 0 && end >= 0, "invalid range [%d %d]", begin, end);
    if (begin != end) TODO();
    Assert(begin <= end && end < (int)arrayMember.size(), "invalid range [%d, %d] in node %s", begin, end, name.c_str());
    getArrayMember(begin)->valTree = tree;
  }
  arrayVal.clear();
  /* clear all connections of current array */
  std::set<Node*> prevNodes(prev);
  std::set<Node*> nextNodes(next);
  prev.clear();
  next.clear();
  /* update node prev & next */
  for (Node* node : prevNodes) {
    node->next.erase(this);
  }
  for (Node* node : nextNodes) {
    node->prev.erase(this);
  }
  /* don't need to update the connections of prevNodes, as their valTrees remain unchanged */
  for (Node* node : arrayMember) node->updateConnect();
  for (Node* node : nextNodes) node->updateConnect();

  /* update super prev & next */
  for (Node* node : prevNodes) node->super->next.erase(super);
  for (Node* node : nextNodes) node->super->prev.erase(super);
  constructSuperConnect();
  for (Node* member : arrayMember) member->constructSuperConnect();

  /* revise astTree in its previous nodes: (*not needed) */

  /* add connection between member and array(member->array) to garentee assignment of member precede the usage of array */

  for (Node* member : arrayMember) {
    member->next.insert(this);
    this->prev.insert(member);
    this->super->add_prev(member->super);
  }

}

std::string Node::arrayMemberName(int idx) {
  Assert(isArray(), "%s is not array", name.c_str());
  std::vector<int>index(dimension);
  int dividend = dimension.back();
  int divisor = idx;

  for (int i = (int)dimension.size() - 1; i >= 0; i --) {
    dividend = dimension[i];
    index[i] = divisor % dividend;
    divisor = divisor / dividend;
  }

  std::string ret = name;
  for (size_t i = 0; i < index.size(); i ++) ret += "__" + std::to_string(index[i]);
  return ret;
}