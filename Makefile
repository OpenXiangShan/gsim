##############################################
### User Settings
##############################################

dutName ?= ysyx3
MODE ?= 0
# uncomment this line to let this file be part of dependency of each .o file
THIS_MAKEFILE = Makefile

ifeq ($(dutName),ysyx3)
	NAME ?= newtop
	mainargs = ready-to-run/bin/bbl-hello.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-rocket.bin
	TEST_FILE = $(NAME)-ysyx3
	GSIM_FLAGS += --supernode-max-size=20
else ifeq ($(dutName),NutShell)
	NAME ?= SimTop
	mainargs = ready-to-run/bin/linux-NutShell.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-NutShell.bin
	TEST_FILE = $(NAME)-nutshell
	GSIM_FLAGS += --supernode-max-size=20
else ifeq ($(dutName),rocket)
	NAME ?= TestHarness
	mainargs = ready-to-run/bin/linux-rocket.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-rocket.bin
	TEST_FILE = $(NAME)-rocket
	GSIM_FLAGS += --supernode-max-size=20
else ifeq ($(dutName),boom)
	NAME ?= TestHarness
	mainargs = ready-to-run/bin/linux-rocket.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-rocket.bin
	TEST_FILE = $(NAME)-LargeBoom
	GSIM_FLAGS += --supernode-max-size=35
else ifeq ($(dutName),small-boom)
	NAME ?= TestHarness
	mainargs = ready-to-run/bin/linux-rocket.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-rocket.bin
	TEST_FILE = $(NAME)-SmallBoom
	GSIM_FLAGS += --supernode-max-size=35
else ifeq ($(dutName),xiangshan)
	NAME ?= SimTop
	mainargs = ready-to-run/bin/linux-xiangshan-202501.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-NutShell.bin
	TEST_FILE = $(NAME)-xiangshan-minimal-202501-20957846
	GSIM_FLAGS += --supernode-max-size=35
else ifeq ($(dutName),xiangshan-default)
	NAME ?= SimTop
	mainargs = ready-to-run/bin/linux-xiangshan-202501.bin
	PGO_WORKLOAD ?= ready-to-run/bin/microbench-NutShell.bin
	TEST_FILE = $(NAME)-xiangshan-default-202501-20957846
	GSIM_FLAGS += --supernode-max-size=35
endif

##############################################
### Global Settings
##############################################

BUILD_DIR ?= build
WORK_DIR = $(BUILD_DIR)/$(TEST_FILE)
CXX = clang++
CCACHE = ccache
SHELL := /bin/bash

CFLAGS_DUT = -DDUT_NAME=S$(NAME) -DDUT_HEADER=\"$(NAME).h\" -D__DUT_$(shell echo $(dutName) | tr - _)__

# $(1): object file
# $(2): source file
# $(3): compile flags
# $(4): object file list
# $(5): extra dependency
define CXX_TEMPLATE =
$(1): $(2) $(THIS_MAKEFILE) $(5)
	@mkdir -p $$(@D) && echo + CXX $$<
	@$(CCACHE) $(CXX) $$< $(3) -c -o $$@
$(4) += $(1)
endef

# $(1): object file
# $(2): source file
# $(3): ld flags
define LD_TEMPLATE =
$(1): $(2)
	@mkdir -p $$(@D) && echo + LD $$@
	@$(CXX) $$^ $(3) -o $$@
endef

ifeq ($(PERF),1)
	CXXFLAGS += -DPERF
	MODE_FLAGS += -DGSIM
	EMU_CFLAGS += -DPERF -DACTIVE_FILE=\"logs/active-$(dutName).txt\" -O0 -Wno-format
	target ?= run-emu
else ifeq ($(MODE),0)
	MODE_FLAGS += -DGSIM
	EMU_CFLAGS += -O3 -Wno-format $(PGO_CFLAGS)
	target ?= run-emu
else ifeq ($(MODE), 1)
	MODE_FLAGS += -DVERILATOR
	target ?= run-veri
else ifeq ($(MODE), 2)
	CXXFLAGS += -DDIFFTEST_PER_SIG -DVERILATOR_DIFF
	MODE_FLAGS += -DGSIM -DVERILATOR
	SIG_COMMAND = python3 scripts/sigFilter.py $(WORK_DIR) $(NAME)
	target ?= run-veri
else
	CXXFLAGS += -DDIFFTEST_PER_SIG -DGSIM_DIFF
	MODE_FLAGS += -DGSIM -DGSIM_DIFF
	EMU_CFLAGS += -O0
	EMU_CFLAGS += -I$(REF_GSIM_DIR) -DREF_NAME=Diff$(NAME)
	SIG_COMMAND = python3 scripts/genSigDiff.py $(NAME) $(DIFF_VERSION)
	target ?= $(EMU_BUILD_DIR)/S$(NAME)_diff
endif

# Pass from outside Design or internal Default
ifdef GSIM_TARGET
target = $(GSIM_TARGET)
endif

difftest: $(target)

.PHONY: difftest

##############################################
### Building GSIM
##############################################

GSIM_BUILD_DIR = $(BUILD_DIR)/gsim
GSIM_BIN = $(GSIM_BUILD_DIR)/gsim

PARSER_DIR = parser
LEXICAL_NAME = lexical
SYNTAX_NAME = syntax
PARSER_BUILD_DIR = $(GSIM_BUILD_DIR)/$(PARSER_DIR)
PARSER_GEN_SRCS = $(foreach x, $(LEXICAL_NAME) $(SYNTAX_NAME), $(PARSER_BUILD_DIR)/$(x).cc)
GSIM_SRCS = $(foreach x, src $(PARSER_DIR), $(wildcard $(x)/*.cpp))

GSIM_INC_DIR = include $(PARSER_DIR)/include $(PARSER_BUILD_DIR)
CXXFLAGS += -ggdb -O3 -MMD $(addprefix -I,$(GSIM_INC_DIR)) -Wall -Werror --std=c++17

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

$(PARSER_BUILD_DIR)/%.cc $(PARSER_BUILD_DIR)/%.hh: $(PARSER_DIR)/%.y
	@mkdir -p $(@D)
	bison -v -d $< -o $@

$(PARSER_BUILD_DIR)/%.cc: $(PARSER_DIR)/%.l
	@mkdir -p $(@D)
	flex -Cf -o $@ $<

$(foreach x, $(PARSER_GEN_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(PARSER_BUILD_DIR)/$(basename $(notdir $(x))).o, $(x), $(CXXFLAGS), GSIM_OBJS,)))

$(foreach x, $(GSIM_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(GSIM_BUILD_DIR)/$(basename $(x)).o, $(x), $(CXXFLAGS), GSIM_OBJS,)))

$(eval $(call LD_TEMPLATE, $(GSIM_BIN), $(GSIM_OBJS), $(CXXFLAGS) -lgmp))

# Dependency
-include $(GSIM_OBJS:.o=.d)

##############################################
### Running GSIM to generate cpp model
##############################################

FIRRTL_FILE = ready-to-run/$(TEST_FILE).fir
GEN_CPP_DIR = $(WORK_DIR)/model
SPLIT_CPP_DIR = $(GEN_CPP_DIR)/split

$(GEN_CPP_DIR)/$(NAME).cpp: $(GSIM_BIN) $(FIRRTL_FILE)
	@mkdir -p $(@D)
	set -o pipefail && /bin/time $(GSIM_BIN) $(GSIM_FLAGS) --dir $(@D) $(FIRRTL_FILE) | tee $(BUILD_DIR)/gsim.log

$(SPLIT_CPP_DIR)/$(NAME)0.cpp: $(GEN_CPP_DIR)/$(NAME).cpp
	-rm -rf $(@D)
	mkdir -p $(@D)
	$(SIG_COMMAND)
	python ./scripts/partition.py $< $(@D)

compile: $(SPLIT_CPP_DIR)/$(NAME)0.cpp

.PHONY: compile

##############################################
### Building EMU from cpp model generated by GSIM
##############################################

EMU_BUILD_DIR = $(WORK_DIR)/emu
EMU_BIN = $(WORK_DIR)/S$(NAME)

EMU_LIB_DIR = emu/lib
EMU_MAIN_SRCS = emu/emu.cpp $(shell find $(EMU_LIB_DIR) -name "*.cpp")
EMU_GEN_SRCS = $(shell find $(SPLIT_CPP_DIR) -name "*.cpp" 2> /dev/null)
EMU_SRCS = $(EMU_MAIN_SRCS) $(EMU_GEN_SRCS)

EMU_INC_DIR = $(GEN_CPP_DIR) $(EMU_LIB_DIR)
EMU_CFLAGS := -O3 -MMD $(addprefix -I, $(abspath $(EMU_INC_DIR))) $(EMU_CFLAGS) # allow to overwrite -O3
EMU_CFLAGS += $(MODE_FLAGS) $(CFLAGS_DUT) -Wno-parentheses-equality
EMU_CFLAGS += -fbracket-depth=2048
#EMU_CFLAGS += -fsanitize=address -fsanitize-address-use-after-scope
#EMU_CFLAGS += -fsanitize=undefined -fsanitize=pointer-compare -fsanitize=pointer-subtract
#EMU_CFLAGS += -pg -ggdb

$(foreach x, $(EMU_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(EMU_BUILD_DIR)/$(basename $(notdir $(x))).o, $(x), $(EMU_CFLAGS), EMU_OBJS,)))

$(eval $(call LD_TEMPLATE, $(EMU_BIN), $(EMU_OBJS), $(EMU_CFLAGS) -lgmp))

run-emu: $(EMU_BIN)
	$^ $(mainargs)

clean-emu:
	-rm -rf $(EMU_BUILD_DIR) $(EMU_BIN)

# Dependency
-include $(EMU_OBJS:.o=.d)

.PHONY: run-emu clean-emu

##############################################
### Building EMU from Verilator
##############################################

CFLAGS_REF = -DREF_NAME=V$(NAME) -DREF_HEADER=\"V$(NAME)__Syms.h\"

define escape_quote
	$(subst \",\\\",$(1))
endef

VERI_BUILD_DIR = $(WORK_DIR)/verilator
VERI_BIN = $(WORK_DIR)/V$(NAME)
VERI_GEN_MK = $(VERI_BUILD_DIR)/V$(NAME).mk

VERI_CFLAGS = $(call escape_quote,$(EMU_CFLAGS) $(CFLAGS_REF))
VERI_LDFLAGS = -O3 -lgmp
VERI_VFLAGS = --top $(NAME) -O3 -Wno-lint -j 8 --cc --exe +define+RANDOMIZE_GARBAGE_ASSIGN --max-num-width 1048576 --compiler clang
VERI_VFLAGS += -Mdir $(VERI_BUILD_DIR) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)"
#VERI_VFLAGS += --trace-fst

VERI_VSRCS = ready-to-run/difftest/$(TEST_FILE).sv
VERI_VSRCS += $(shell find ready-to-run/difftest/blockbox/ -name ".v")

$(VERI_GEN_MK): $(VERI_VSRCS) $(EMU_GEN_SRCS) | $(EMU_MAIN_SRCS)
	@mkdir -p $(@D)
	verilator $(VERI_VFLAGS) $(abspath $^ $|)

$(VERI_BIN): | $(VERI_GEN_MK)
	$(MAKE) OPT_FAST="-O3" CXX=clang++ -s -C $(VERI_BUILD_DIR) -f $(abspath $|)
	ln -sf $(abspath $(VERI_BUILD_DIR)/V$(NAME)) $@

compile-veri: $(VERI_GEN_MK)

run-veri: $(VERI_BIN)
	$^ $(mainargs)

.PHONY: compile-veri run-veri $(VERI_BIN)

##############################################
### Building EMU from cpp model generated by GSIM for reference
##############################################

DIFF_VERSION ?= 2024_1_14
# DIFF_VERSION ?= NutShell

REF_GSIM_DIR = $(EMU_SRC_DIR)/obj_$(DIFF_VERSION)
REF_GSIM_SRCS = $(shell find $(REF_GSIM_DIR)/splitted -name "*.cpp")
REF_GSIM_OBJS = $(addprefix $(EMU_BUILD_DIR)/, $(REF_GSIM_SRCS:.cpp=.o))

#$(EMU_BUILD_DIR)/S$(NAME)_diff: $(VERI_OBJS) $(REF_GSIM_OBJS)
#	echo $(REF_GSIM_OBJS)
#	$(CXX) $^ $(EMU_CFLAGS) -lgmp -o $@
#
#$(GSIM_TARGET): $(VERI_OBJS)
#	$(CXX) $^ $(EMU_CFLAGS) $(EMU_LDFLAGS) -lgmp -o $@


##############################################
### PGO
##############################################

LLVM_PROFDATA = llvm-profdata
PGO_BUILD_DIR = $(WORK_DIR)/pgo

build-emu-pgo:
	rm -rf $(PGO_BUILD_DIR)
	mkdir -p $(PGO_BUILD_DIR)
	make clean-pgo
	make build-emu PGO_CFLAGS="-fprofile-generate=`realpath $(PGO_BUILD_DIR)`"
	./scripts/run_until_exit.sh $(target) $(PGO_WORKLOAD)
ifneq ($(LLVM_PROFDATA),)
	$(LLVM_PROFDATA) merge -o $(PGO_BUILD_DIR)/default.profdata $(PGO_BUILD_DIR)/*.profraw
endif
	make clean-pgo
	make build-emu PGO_CFLAGS="-fprofile-use=$(PGO_BUILD_DIR)/default.profdata"

clean-pgo:
	-rm -rf $(EMU_BUILD_DIR)

.PHONY: build-emu-pgo clean-pgo

##############################################
### Miscellaneous
##############################################

clean:
	-rm -rf $(BUILD_DIR)

run:
	$(MAKE) MODE=0 compile
	$(MAKE) MODE=0 difftest

diff:
	$(MAKE) MODE=2 compile-veri
	$(MAKE) MODE=2 compile
	$(MAKE) MODE=2 difftest

unzip:
	cd ready-to-run && tar xvjf xiangshan.tar.bz2
	cd ready-to-run/difftest && tar xvjf xiangshan.tar.bz2

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

format-obj:
	@clang-format -i --style=file obj/$(NAME).cpp

.PHONY: clean run diff unzip perf count gendoc format-obj
