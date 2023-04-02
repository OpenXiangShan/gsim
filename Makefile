BUILD_DIR = ./build

PARSER_DIR = ./parser
LEXICAL_SRC = $(PARSER_DIR)/lexical.l
SYNTAX_SRC = $(PARSER_DIR)/syntax.y
PARSER_BUILD = $(PARSER_DIR)/build

SRCS = $(shell find src $(PARSER_DIR) -name "*.cpp" -o -name "*.cc")
INCLUDE_DIR = include $(PARSER_BUILD) $(PARSER_DIR)/include

CXXFLAGS = -O2 -DOBJ_DIR=\"obj\" $(addprefix -I,$(INCLUDE_DIR))
CXX = g++
TARGET = GraphEmu

FIRRTL_FILE = scala/build/_add.lo.fir

EMU_SRC = emu/simadd.cpp
EMU_TARGET = emu_test

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif


run: $(PARSER_BUILD)/syntax.cc
	mkdir -p build
	mkdir -p obj
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD_DIR)/$(TARGET)
	$(BUILD_DIR)/$(TARGET) $(FIRRTL_FILE)

clean:
	rm -rf obj
	rm -rf parser/build

emu: obj/top.cpp $(EMU_SRC)
	g++ $(EMU_SRC) obj/top.cpp -Iobj -o $(BUILD_DIR)/$(EMU_TARGET)
	$(BUILD_DIR)/$(EMU_TARGET)

$(PARSER_BUILD)/syntax.cc: $(LEXICAL_SRC) $(SYNTAX_SRC)
	mkdir -p $(PARSER_BUILD)
	flex -o $(PARSER_BUILD)/lexical.cc $(LEXICAL_SRC)
	bison -v -d $(SYNTAX_SRC) -o $(PARSER_BUILD)/syntax.cc

.PHONY: run clean emu