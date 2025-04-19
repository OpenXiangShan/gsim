##############################################
### User Settings
##############################################

dutName ?= ysyx3
MODE ?= 0
mainargs ?= ready-to-run/bin/linux.bin
# uncomment this line to let this file be part of dependency of each .o file
THIS_MAKEFILE = Makefile

ifeq ($(dutName),ysyx3)
	NAME ?= newtop
	TEST_FILE = $(NAME)-ysyx3
	GSIM_FLAGS += --supernode-max-size=20
else ifeq ($(dutName),NutShell)
	NAME ?= SimTop
	TEST_FILE = $(NAME)-nutshell
	GSIM_FLAGS += --supernode-max-size=20
else ifeq ($(dutName),rocket)
	NAME ?= TestHarness
	TEST_FILE = $(NAME)-rocket
	GSIM_FLAGS += --supernode-max-size=20 --cpp-max-size-KB=1024
else ifeq ($(dutName),large-boom)
	NAME ?= TestHarness
	TEST_FILE = $(NAME)-LargeBoom
	GSIM_FLAGS += --supernode-max-size=35 --cpp-max-size-KB=4096
	VERI_THREADS = --threads 5
else ifeq ($(dutName),small-boom)
	NAME ?= TestHarness
	TEST_FILE = $(NAME)-SmallBoom
	GSIM_FLAGS += --supernode-max-size=35 --cpp-max-size-KB=4096
	VERI_THREADS = --threads 5
else ifeq ($(dutName),minimal-xiangshan)
	NAME ?= SimTop
	TEST_FILE = $(NAME)-xiangshan-minimal
	GSIM_FLAGS += --supernode-max-size=35 --cpp-max-size-KB=8192
	VERI_THREADS = --threads 16
else ifeq ($(dutName),default-xiangshan)
	NAME ?= SimTop
	TEST_FILE = $(NAME)-xiangshan-default
	GSIM_FLAGS += --supernode-max-size=35 --cpp-max-size-KB=8192
	VERI_THREADS = --threads 16
endif

##############################################
### Global Settings
##############################################

BUILD_DIR ?= build
WORK_DIR = $(BUILD_DIR)/$(dutName)
CXX = clang++

SHELL := /bin/bash
TIME = /usr/bin/time
LOG_FILE = $(WORK_DIR)/$(dutName).log

CFLAGS_DUT = -DDUT_NAME=S$(NAME) -DDUT_HEADER=\"$(NAME).h\" -D__DUT_$(shell echo $(dutName) | tr - _)__

# $(1): object file
# $(2): source file
# $(3): compile flags
# $(4): object file list
# $(5): extra dependency
define CXX_TEMPLATE =
$(1): $(2) $(THIS_MAKEFILE) $(5)
	@mkdir -p $$(@D) && echo + CXX $$<
	@$(CXX) $$< $(3) -c -o $$@
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
PARSER_GEN_HEADER = $(PARSER_BUILD_DIR)/$(SYNTAX_NAME).hh
GSIM_SRCS = $(foreach x, src $(PARSER_DIR), $(wildcard $(x)/*.cpp))

GSIM_INC_DIR = include $(PARSER_DIR)/include $(PARSER_BUILD_DIR)
CXXFLAGS += -ggdb -O3 -MMD $(addprefix -I,$(GSIM_INC_DIR)) -Wall -Werror --std=c++20
# libstdc on Debian Bookworm is too old, remove this when Debian Trixie is released
ifeq ($(shell grep -m1 -o 'Debian' /etc/os-release 2>/dev/null),Debian)
    CXXFLAGS += -stdlib=libc++
endif

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

$(PARSER_BUILD_DIR)/%.cc:  $(PARSER_DIR)/%.y
	@mkdir -p $(@D)
	bison -v -d $< -o $@

$(PARSER_BUILD_DIR)/%.cc: $(PARSER_DIR)/%.l
	@mkdir -p $(@D)
	flex -Cf -o $@ $<

$(PARSER_GEN_HEADER): $(PARSER_BUILD_DIR)/$(SYNTAX_NAME).cc

$(foreach x, $(PARSER_GEN_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(PARSER_BUILD_DIR)/$(basename $(notdir $(x))).o, $(x), $(CXXFLAGS), GSIM_OBJS, $(PARSER_GEN_HEADER))))

$(foreach x, $(GSIM_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(GSIM_BUILD_DIR)/$(basename $(x)).o, $(x), $(CXXFLAGS), GSIM_OBJS, $(PARSER_GEN_HEADER))))

$(eval $(call LD_TEMPLATE, $(GSIM_BIN), $(GSIM_OBJS), $(CXXFLAGS) -lgmp))

build-gsim: $(GSIM_BIN)

# Dependency
-include $(GSIM_OBJS:.o=.d)

.PHONY: build-gsim

##############################################
### Running GSIM to generate cpp model
##############################################

FIRRTL_FILE = ready-to-run/$(TEST_FILE).fir
GEN_CPP_DIR = $(WORK_DIR)/model

$(GEN_CPP_DIR)/$(NAME)0.cpp: $(GSIM_BIN) $(FIRRTL_FILE)
	@mkdir -p $(@D)
	set -o pipefail && $(TIME) $(GSIM_BIN) $(GSIM_FLAGS) --dir $(@D) $(FIRRTL_FILE) | tee $(BUILD_DIR)/gsim.log
	$(SIG_COMMAND)

compile: $(GEN_CPP_DIR)/$(NAME)0.cpp

.PHONY: compile

##############################################
### Building EMU from cpp model generated by GSIM
##############################################

EMU_BUILD_DIR = $(WORK_DIR)/emu
EMU_BIN = $(WORK_DIR)/S$(NAME)

EMU_MAIN_SRCS = emu/emu.cpp
EMU_GEN_SRCS = $(shell find $(GEN_CPP_DIR) -name "*.cpp" 2> /dev/null)
EMU_SRCS = $(EMU_MAIN_SRCS) $(EMU_GEN_SRCS)

EMU_CFLAGS := -O1 -MMD $(addprefix -I, $(abspath $(GEN_CPP_DIR))) $(EMU_CFLAGS) # allow to overwrite optimization level
EMU_CFLAGS += $(MODE_FLAGS) $(CFLAGS_DUT) -Wno-parentheses-equality
EMU_CFLAGS += -fbracket-depth=2048
#EMU_CFLAGS += -fsanitize=address -fsanitize-address-use-after-scope
#EMU_CFLAGS += -fsanitize=undefined -fsanitize=pointer-compare -fsanitize=pointer-subtract
#EMU_CFLAGS += -pg -ggdb

$(foreach x, $(EMU_SRCS), $(eval \
	$(call CXX_TEMPLATE, $(EMU_BUILD_DIR)/$(basename $(notdir $(x))).o, $(x), $(EMU_CFLAGS), EMU_OBJS,)))

$(eval $(call LD_TEMPLATE, $(EMU_BIN), $(EMU_OBJS), $(EMU_CFLAGS)))

run-emu: $(EMU_BIN)
	$(TIME) taskset 0x1 $^ $(mainargs)

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
VERI_LDFLAGS = -O1
VERI_VFLAGS = --top $(NAME) -Wno-lint -j 8 --cc --exe +define+RANDOMIZE_GARBAGE_ASSIGN --max-num-width 1048576 --compiler clang
VERI_VFLAGS += -Mdir $(VERI_BUILD_DIR) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)"
VERI_VFLAGS += $(VERI_THREADS)
#VERI_VFLAGS += --trace-fst

VERI_VSRCS = ready-to-run/difftest/$(TEST_FILE).sv
VERI_CSRCS-2 = $(EMU_GEN_SRCS)

$(VERI_GEN_MK): $(VERI_VSRCS) $(VERI_CSRCS-$(MODE)) | $(EMU_MAIN_SRCS)
	@mkdir -p $(@D)
	verilator $(VERI_VFLAGS) $(abspath $^ $|)

$(VERI_BIN): | $(VERI_GEN_MK)
	$(TIME) $(MAKE) OPT_FAST="-O1" CXX=clang++ -s -C $(VERI_BUILD_DIR) -f $(abspath $|)
	ln -sf $(abspath $(VERI_BUILD_DIR)/V$(NAME)) $@

compile-veri: $(VERI_GEN_MK)

run-veri: $(VERI_BIN)
	$(TIME) $^ $(mainargs)

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

pgo:
	rm -rf $(PGO_BUILD_DIR)
	mkdir -p $(PGO_BUILD_DIR)
	$(MAKE) MODE=0 compile
	$(MAKE) clean-emu
	$(MAKE) run MODE=0 PGO_CFLAGS="-fprofile-generate=$(PGO_BUILD_DIR)"
	$(LLVM_PROFDATA) merge -o $(PGO_BUILD_DIR)/default.profdata $(PGO_BUILD_DIR)/*.profraw
	$(MAKE) clean-emu
	$(MAKE) run MODE=0 PGO_CFLAGS="-fprofile-use=$(PGO_BUILD_DIR)/default.profdata"

.PHONY: pgo

##############################################
### Miscellaneous
##############################################

clean:
	-@rm -rf $(BUILD_DIR)

run-internal:
	$(MAKE) MODE=0 compile
	$(MAKE) MODE=0 difftest

diff-internal:
	$(MAKE) MODE=2 compile-veri
	$(MAKE) MODE=2 compile
	$(MAKE) MODE=2 difftest

run:
	mkdir -p $(dir $(LOG_FILE))
	set -o pipefail && $(TIME) $(MAKE) run-internal 2>&1 | tee $(LOG_FILE)

diff:
	mkdir -p $(dir $(LOG_FILE))
	set -o pipefail && $(TIME) $(MAKE) diff-internal 2>&1 | tee $(LOG_FILE)

init:
	git submodule update --init
	cd ready-to-run && bash init.sh

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

.PHONY: clean run-internal diff-internal run diff init perf count gendoc format-obj
