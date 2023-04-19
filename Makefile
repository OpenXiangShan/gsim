BUILD_DIR = ./build

PARSER_DIR = ./parser
LEXICAL_SRC = $(PARSER_DIR)/lexical.l
SYNTAX_SRC = $(PARSER_DIR)/syntax.y
PARSER_BUILD = $(PARSER_DIR)/build

INCLUDE_DIR = include $(PARSER_BUILD) $(PARSER_DIR)/include

OBJ_DIR = obj

CXXFLAGS = -O2 -DOBJ_DIR=\"$(OBJ_DIR)\" $(addprefix -I,$(INCLUDE_DIR)) -lgmp
CXX = g++
TARGET = GraphEmu

NAME ?= GCD
TEST_FILE = scala/build/$(NAME)
FIRRTL_FILE = $(TEST_FILE).lo.fir

EMU_DIR = emu
EMU_SRC = $(EMU_DIR)/difftest.cpp
EMU_TARGET = emu_test
EMU_SRC_DIR = emu-src

SRCS = $(shell find src $(PARSER_DIR) -name "*.cpp" -o -name "*.cc" )

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

VERI_INC_DIR = $(OBJ_DIR) $(EMU_DIR)/include include
VERI_VFLAGS = --exe $(addprefix -I, $(VERI_INC_DIR)) --top $(NAME)
VERI_CFLAGS = -O3 $(addprefix -I../, $(VERI_INC_DIR))
VERI_CFLAGS += -DMOD_NAME=S$(NAME) -DREF_NAME=V$(NAME) -DHEADER=\\\"V$(NAME)__Syms.h\\\"
VERI_LDFLAGS = -O3 -lgmp
VERI_VSRCS = $(TEST_FILE).v
VERI_CSRCS = $(shell find $(OBJ_DIR) -name "*.cpp") $(EMU_DIR)/difftest.cpp

compile: $(PARSER_BUILD)/syntax.cc
	mkdir -p build
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD_DIR)/$(TARGET)
	$(BUILD_DIR)/$(TARGET) $(FIRRTL_FILE)
	cp $(EMU_SRC_DIR)/* $(OBJ_DIR)

clean:
	rm -rf obj parser/build obj_dir

emu: obj/top.cpp $(EMU_SRC) compile
	g++ $(EMU_SRC) obj/top.cpp -Iobj -o $(BUILD_DIR)/$(EMU_TARGET)
	$(BUILD_DIR)/$(EMU_TARGET)

$(PARSER_BUILD)/syntax.cc: $(LEXICAL_SRC) $(SYNTAX_SRC)
	mkdir -p $(PARSER_BUILD)
	flex -o $(PARSER_BUILD)/lexical.cc $(LEXICAL_SRC)
	bison -v -d $(SYNTAX_SRC) -o $(PARSER_BUILD)/syntax.cc

difftest: compile
	verilator $(VERI_VFLAGS) -j 8 --cc $(VERI_VSRCS) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)" $(VERI_CSRCS)
	make -s OPT_FAST="-O3" -j -C ./obj_dir -f V$(NAME).mk V$(NAME)
	./obj_dir/V$(NAME)

.PHONY: compile clean emu difftest