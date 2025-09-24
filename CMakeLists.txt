cmake_minimum_required(VERSION 3.15)
project(gsim LANGUAGES CXX)

# ---------- 1. 找工具 ----------
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

find_library(GMP_LIB gmp REQUIRED)
find_package(Threads REQUIRED)

set(LLVM_DIR "/download/llvm_mlir_circt/circt/install/llvm/lib/cmake/llvm" CACHE PATH "" FORCE)
set(MLIR_DIR "/download/llvm_mlir_circt/circt/install/llvm/lib/cmake/mlir" CACHE PATH "" FORCE)
set(CIRCT_DIR "/download/llvm_mlir_circt/circt/install/circt/lib/cmake/circt" CACHE PATH "" FORCE)
find_package(LLVM REQUIRED CONFIG)
find_package(MLIR REQUIRED CONFIG)
find_package(CIRCT REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
message(STATUS "Found MLIR ${MLIR_PACKAGE_VERSION}")
message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
message(STATUS "Found CIRCT ${CIRCT_PACKAGE_VERSION}")
message(STATUS "Using CIRCTConfig.cmake in: ${CIRCT_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CIRCT_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_DIR}")
include(AddLLVM)
include(AddMLIR)
include(AddCIRCT)
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${CIRCT_INCLUDE_DIRS})
# ---------- 2. 生成文件 ----------
# 让 bison 先跑，产出 parser.cpp + parser.h
bison_target(
  GsimParser
  ${CMAKE_CURRENT_SOURCE_DIR}/parser/syntax.y
  ${CMAKE_CURRENT_BINARY_DIR}/parser/syntax.cpp
  COMPILE_FLAGS "-v -d --defines=${CMAKE_CURRENT_BINARY_DIR}/parser/syntax.hh"   # 生成头文件
)

# 让 flex 跑，产出 lexer.cpp；依赖上面的 parser.h
flex_target(
  GsimLexer
  ${CMAKE_CURRENT_SOURCE_DIR}/parser/lexical.l
  ${CMAKE_CURRENT_BINARY_DIR}/parser/lexical.cpp
  COMPILE_FLAGS "-Cf"
)

# 把两个 target 绑定到同一个名字，后面直接引用
add_flex_bison_dependency(GsimLexer GsimParser)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
     src/*.cpp parser/*.cpp)   # 现有源文件

add_executable(${PROJECT_NAME}
  ${SRC_FILES}
  ${BISON_GsimParser_OUTPUTS}
  ${FLEX_GsimLexer_OUTPUTS}
)

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "-fexceptions -fno-rtti")

target_link_libraries(${PROJECT_NAME} PRIVATE ${GMP_LIB} Threads::Threads)
target_link_libraries(${PROJECT_NAME} PRIVATE MLIRIR MLIRParser MLIRPass MLIRSupport 
                                              CIRCTHW CIRCTComb CIRCTSV CIRCTSeq CIRCTEmit CIRCTOM
                                              LLVMSupport LLVMCore)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/parser/include  
  ${CMAKE_CURRENT_BINARY_DIR}/parser
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

