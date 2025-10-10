/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/
#ifndef CIRCT2GRAPH_H
#define CIRCT2GRAPH_H
#include "common.h"
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/SV/SVOps.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Dialect/Emit/EmitDialect.h"
#include "circt/Dialect/Emit/EmitOps.h"
#include "circt/Dialect/OM/OMDialect.h"
#include "circt/Dialect/OM/OMOps.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "llvm/ADT/DenseMap.h"
#include <map>
using namespace mlir;
using namespace circt;

class CIRCT2Graph {
 public:
  CIRCT2Graph(ModuleOp module) {
    module->walk([&](hw::HWModuleOp hwModuleOp) { topModule = hwModuleOp; });
  }
  graph* generateGraph();
 private:
  graph* g;
  hw::HWModuleOp topModule;
  llvm::DenseMap<mlir::Value, Node*> valueNodeMap;

  void processInputPort();
  void processOperations();
  void processConstantOp(hw::ConstantOp constantOp);
};

#endif
