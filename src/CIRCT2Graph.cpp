/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#include "CIRCT2Graph.h"

graph* CIRCT2Graph::generateGraph() {
  g = new graph();
  processInputPort();
  return g;
}

void CIRCT2Graph::processInputPort() {
  size_t inputPortIdx = 0;
  for(size_t i = 0; i < topModule.getNumPorts(); i++) {
    auto portInfo = topModule.getPort(i);
    if(portInfo.isOutput()) continue;
    // 创建 typeInfo
    TypeInfo* typeInfo = new TypeInfo();
    typeInfo->set_sign(portInfo.type.isSignedInteger());
    typeInfo->set_width(portInfo.type.getIntOrFloatBitWidth());
    typeInfo->set_reset(UNCERTAIN);
    // 创建 node
    Node* node = new Node(NODE_INP); 
    node->name = topModule.getPortName(i).str();
    node->updateInfo(typeInfo);
    // 将 node 添加到图中
    g->input.push_back(node);
    // 将 value-node 关系添加到 map 中
    mlir::Value portValue = topModule.getBodyRegion().front().getArgument(inputPortIdx);
    valueNodeMap[portValue] = node;
    inputPortIdx++;
  }
}

void CIRCT2Graph::processOperations() {

}

