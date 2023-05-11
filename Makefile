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

NAME ?= newtop
TEST_FILE = scala/build/$(NAME)
FIRRTL_FILE = $(TEST_FILE).lo.fir

EMU_DIR = emu
EMU_SRC = $(EMU_DIR)/emu.cpp $(shell find $(EMU_SRC_DIR) -name "*.cpp")
EMU_TARGET = emu_test
EMU_SRC_DIR = emu-src

SRCS = $(shell find src $(PARSER_DIR) -name "*.cpp" -o -name "*.cc" )

MODE ?= 2

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

VERI_INC_DIR = $(OBJ_DIR) $(EMU_DIR)/include include $(EMU_SRC_DIR)
VERI_VFLAGS = --exe $(addprefix -I, $(VERI_INC_DIR)) --top $(NAME)
VERI_CFLAGS = -O3 $(addprefix -I../, $(VERI_INC_DIR))
VERI_CFLAGS += -DMOD_NAME=S$(NAME) -DREF_NAME=V$(NAME) -DHEADER=\\\"V$(NAME)__Syms.h\\\"
VERI_LDFLAGS = -O3 -lgmp
VERI_VSRCS = $(TEST_FILE).v
VERI_VSRCS += $(addprefix scala/build/, SdCard.v TransExcep.v UpdateCsrs.v UpdateRegs.v InstFinish.v)
VERI_CSRCS = $(shell find $(OBJ_DIR) $(EMU_SRC_DIR) -name "*.cpp") $(EMU_DIR)/difftest-ysyx3.cpp

ifeq ($(MODE),0)
	VERI_CFLAGS += -DGSIM
else ifeq ($(MODE), 1)
	VERI_CFLAGS += -DVERILATOR
else
	VERI_CFLAGS += -DGSIM -DVERILATOR
endif

mainargs = ysyx3-bin/rtthread.bin

compile: $(PARSER_BUILD)/syntax.cc
	mkdir -p build
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD_DIR)/$(TARGET)
	$(BUILD_DIR)/$(TARGET) $(FIRRTL_FILE)

clean:
	rm -rf obj parser/build obj_dir

emu: obj/top.cpp $(EMU_SRC) compile
	g++ $(EMU_SRC) obj/top.cpp -DMOD_NAME=S$(NAME) -Wl,-lgmp -Iobj -I$(EMU_SRC_DIR) -o $(BUILD_DIR)/$(EMU_TARGET)
	$(BUILD_DIR)/$(EMU_TARGET)

$(PARSER_BUILD)/syntax.cc: $(LEXICAL_SRC) $(SYNTAX_SRC)
	mkdir -p $(PARSER_BUILD)
	flex -o $(PARSER_BUILD)/lexical.cc $(LEXICAL_SRC)
	bison -v -d $(SYNTAX_SRC) -o $(PARSER_BUILD)/syntax.cc

difftest:
	verilator $(VERI_VFLAGS) -Wno-lint -j 8 --cc $(VERI_VSRCS) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)" $(VERI_CSRCS)
	python3 scripts/sigFilter.py
	make -s OPT_FAST="-O3" -j -C ./obj_dir -f V$(NAME).mk V$(NAME)
	./obj_dir/V$(NAME) $(mainargs)

.PHONY: compile clean emu difftest