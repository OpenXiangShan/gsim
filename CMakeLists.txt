cmake_minimum_required(VERSION 3.15)
project(gsim LANGUAGES CXX)

# ---------- 1. 找工具 ----------
find_package(FLEX REQUIRED)
find_package(BISON REQUIRED)

find_library(GMP_LIB gmp REQUIRED)
find_package(Threads REQUIRED)

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

target_link_libraries(${PROJECT_NAME} PUBLIC ${GMP_LIB} Threads::Threads)

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/parser/include  
  ${CMAKE_CURRENT_BINARY_DIR}/parser
)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

