BUILD_DIR = build
GSIM_BUILD_DIR = $(BUILD_DIR)/gsim
EMU_BUILD_DIR = $(BUILD_DIR)/emu

PARSER_DIR = parser
LEXICAL_NAME = lexical
SYNTAX_NAME = syntax
PARSER_BUILD = $(PARSER_DIR)/build
FIRRTL_VERSION = 
$(shell mkdir -p $(PARSER_BUILD))

INCLUDE_DIR = include $(PARSER_BUILD) $(PARSER_DIR)/include

OBJ_DIR = obj
$(shell mkdir -p $(OBJ_DIR))

# NAME ?= newtop
# NAME ?= freechips.rocketchip.system.DefaultConfig
# NAME ?= Exp1AllTest
NAME ?= SimTop

# EMU_DIFFTEST = $(EMU_DIR)/difftest-ysyx3.cpp
EMU_DIFFTEST = $(EMU_DIR)/emu-NutShell.cpp

MODE ?= 0
DIFF_VERSION ?= 2023_10_11
EVENT_DRIVEN ?= 0

TEST_FILE = ready-to-run/$(NAME)

CXXFLAGS = -ggdb -O3 -DOBJ_DIR=\"$(OBJ_DIR)\" $(addprefix -I,$(INCLUDE_DIR)) -Wall -Werror \
	-DDST_NAME=\"$(NAME)\" -DEVENT_DRIVEN=$(EVENT_DRIVEN)
CXX = g++
TARGET = GraphEmu

FIRRTL_FILE = $(TEST_FILE).hi.fir

EMU_DIR = emu
EMU_SRC = $(EMU_DIR)/emu.cpp $(shell find $(EMU_SRC_DIR) -name "*.cpp")
EMU_TARGET = emu_test
EMU_SRC_DIR = emu-src

SRC_PATH := src $(PARSER_DIR)

SRCS := $(foreach x, $(SRC_PATH), $(wildcard $(x)/*.c*))
OBJS := $(addprefix $(GSIM_BUILD_DIR)/, $(addsuffix .o, $(basename $(SRCS))))
PARSER_SRCS := $(addprefix $(PARSER_BUILD)/, $(addsuffix $(FIRRTL_VERSION).cc, $(LEXICAL_NAME) $(SYNTAX_NAME)))
PARSER_OBJS := $(PARSER_SRCS:.cc=.o)
HEADERS := $(foreach x, $(INCLUDE_DIR), $(wildcard $(addprefix $(x)/*,.h)))

VERI_INC_DIR = $(OBJ_DIR) $(EMU_DIR)/include include $(EMU_SRC_DIR)
VERI_VFLAGS = --exe $(addprefix -I, $(VERI_INC_DIR)) --top $(NAME)
VERI_CFLAGS = -O3 $(addprefix -I../, $(VERI_INC_DIR)) $(MODE_FLAGS)
VERI_CFLAGS += -DMOD_NAME=S$(NAME) -DREF_NAME=V$(NAME) -DHEADER=\\\"V$(NAME)__Syms.h\\\"
VERI_LDFLAGS = -O3 -lgmp
VERI_VSRCS = $(TEST_FILE).v
VERI_VSRCS += $(addprefix ready-to-run/, SdCard.v TransExcep.v UpdateCsrs.v UpdateRegs.v InstFinish.v DifftestMemInitializer.v)
VERI_CSRCS = $(shell find $(OBJ_DIR) $(EMU_SRC_DIR) -name "*.cpp") $(EMU_DIFFTEST)
VERI_HEADER = $(shell find $(OBJ_DIR) -name "*.h")
VERI_OBJS = $(addprefix $(EMU_BUILD_DIR)/, $(VERI_CSRCS:.cpp=.o))

REF_GSIM_DIR = $(EMU_DIR)/obj_$(DIFF_VERSION)
REF_GSIM_SRCS = $(REF_GSIM_DIR)/top_ref.cpp
REF_GSIM_OBJS = $(addprefix $(EMU_BUILD_DIR)/, $(REF_GSIM_SRCS:.cpp=.o))

GSIM_CFLAGS = -O3 $(addprefix -I, $(VERI_INC_DIR)) $(MODE_FLAGS) -DMOD_NAME=S$(NAME)

mainargs = ready-to-run/bin/linux-NutShell.bin
# mainargs = ready-to-run/bin/bbl-hello.bin
# mainargs = ysyx3-bin/rtthread.bin

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

ifeq ($(MODE),0)
	MODE_FLAGS += -DGSIM
	target = $(EMU_BUILD_DIR)/S$(NAME)
else ifeq ($(MODE), 1)
	MODE_FLAGS += -DVERILATOR
	target = ./obj_dir/V$(NAME)
	VERI_CSRCS = $(EMU_DIFFTEST)
else ifeq ($(MODE), 2)
	MODE_FLAGS += -DGSIM -DVERILATOR
	CXXFLAGS += -DDIFFTEST_PER_SIG
	target = ./obj_dir/V$(NAME)
	SIG_COMMAND = python3 scripts/sigFilter.py
else
	MODE_FLAGS += -DGSIM -DGSIM_DIFF
	target = $(EMU_BUILD_DIR)/S$(NAME)_diff
	CXXFLAGS += -DDIFFTEST_PER_SIG
	GSIM_CFLAGS += -I$(REF_GSIM_DIR) -DREF_NAME=DiffNewTop
	SIG_COMMAND = python3 scripts/genSigDiff.py
endif

$(GSIM_BUILD_DIR)/%.o: %.cpp $(PARSER_SRCS)
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(EMU_BUILD_DIR)/%.o: %.cpp $(VERI_HEADER)
	@mkdir -p $(dir $@) && echo + CXX $<
	$(CXX) $< $(GSIM_CFLAGS) -c -o $@

$(TARGET): makedir $(PARSER_OBJS) $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(PARSER_OBJS) -o $(GSIM_BUILD_DIR)/$(TARGET) -lgmp

makedir:
	mkdir -p build
	mkdir -p build/gsim build/emu

compile: $(TARGET)
	$(GSIM_BUILD_DIR)/$(TARGET) $(FIRRTL_FILE)
	$(SIG_COMMAND)

clean:
	rm -rf obj parser/build obj_dir build

clean-obj:
	rm -rf obj_dir

emu: obj/top.cpp $(EMU_SRC)
	g++ $(EMU_SRC) obj/top.cpp -DMOD_NAME=S$(NAME) -Wl,-lgmp -Iobj -I$(EMU_SRC_DIR) -o $(GSIM_BUILD_DIR)/$(EMU_TARGET)
	$(GSIM_BUILD_DIR)/$(EMU_TARGET)

# flex & bison
$(PARSER_BUILD)/%.cc:  $(PARSER_DIR)/%.y
	bison -v -d $< -o $@

$(PARSER_BUILD)/%.cc: $(PARSER_DIR)/%.l
	flex -o $@ $<

$(PARSER_BUILD)/%.o: $(PARSER_BUILD)/%.cc $(PARSER_SRCS)
	@echo + CXX $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(EMU_BUILD_DIR)/S$(NAME): $(VERI_OBJS)
	$(CXX) $^ $(GSIM_CFLAGS) -lgmp -o $@

$(EMU_BUILD_DIR)/S$(NAME)_diff: $(VERI_OBJS) $(REF_GSIM_OBJS)
	echo $(REF_GSIM_OBJS)
	$(CXX) $^ $(GSIM_CFLAGS) -lgmp -o $@

./obj_dir/V$(NAME): $(VERI_CSRCS)
	verilator $(VERI_VFLAGS) -Wno-lint -j 8 --cc $(VERI_VSRCS) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)" $^
	make -s OPT_FAST="-O3" -j -C ./obj_dir -f V$(NAME).mk V$(NAME)

build-emu: $(target)

difftest: $(target)
	$(target) $(mainargs)

perf: $(target)
	sudo perf record -e branch-instructions,branch-misses,cache-misses,\
	cache-references,instructions,L1-dcache-load-misses,L1-dcache-loads,\
	L1-dcache-store,L1-icache-load-misses,LLC-load-misses,LLC-loads,\
	LLC-store-misses,LLC-store,branch-load-misses,branch-loads,\
	l2_rqsts.code_rd_miss,l2_rqsts.code_rd_hit,l2_rqsts.pf_hit,\
	l2_rqsts.pf_miss,l2_rqsts.all_demand_miss,l2_rqsts.all_demand_data_rd,\
	l2_rqsts.miss $(target) $(mainargs)

count:
	find emu parser src include emu-src scripts -name "*.cpp" -o -name "*.h" -o -name "*.y" -o -name "*.l" -o -name "*.py" |grep -v "emu/obj*" |xargs wc

gendoc:
	doxygen
	python3 -m http.server 8080 --directory doc/html

# format:
# 	@clang-format -i --style=file $(SRCS) $(HEADERS)

format-obj:
	@clang-format -i --style=file obj/top.cpp

.PHONY: compile clean emu difftest count makedir gendoc format-obj build-emu
