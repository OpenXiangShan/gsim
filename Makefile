BUILD_DIR = build

PARSER_DIR = parser
LEXICAL_SRC = $(PARSER_DIR)/lexical.l
SYNTAX_SRC = $(PARSER_DIR)/syntax.y
PARSER_BUILD = $(PARSER_DIR)/build
$(shell mkdir -p $(PARSER_BUILD))

INCLUDE_DIR = include $(PARSER_BUILD) $(PARSER_DIR)/include

OBJ_DIR = obj
$(shell mkdir -p $(OBJ_DIR))

CXXFLAGS = -ggdb -O3 -DOBJ_DIR=\"$(OBJ_DIR)\" $(addprefix -I,$(INCLUDE_DIR))
CXX = g++
TARGET = GraphEmu

NAME ?= newtop
# NAME ?= freechips.rocketchip.system.DefaultConfig
TEST_FILE = ready-to-run/$(NAME)
FIRRTL_FILE = $(TEST_FILE).lo.fir

EMU_DIR = emu
EMU_SRC = $(EMU_DIR)/emu.cpp $(shell find $(EMU_SRC_DIR) -name "*.cpp")
EMU_TARGET = emu_test
EMU_SRC_DIR = emu-src

SRC_PATH := src $(PARSER_DIR)

SRCS := $(foreach x, $(SRC_PATH), $(wildcard $(addprefix $(x)/*,.c*)))
HEADERS := $(foreach x, $(INCLUDE_DIR), $(wildcard $(addprefix $(x)/*,.h)))
OBJS := $(addprefix $(BUILD_DIR)/, $(addsuffix .o, $(basename $(SRCS))))

MODE ?= 0

ifeq ($(DEBUG),1)
	CXXFLAGS += -DDEBUG
endif

ifeq ($(MODE),0)
	MODE_FLAGS += -DGSIM
	target = $(BUILD_DIR)/S$(NAME)
else ifeq ($(MODE), 1)
	MODE_FLAGS += -DVERILATOR
	target = ./obj_dir/V$(NAME)
else
	MODE_FLAGS += -DGSIM -DVERILATOR
	CXXFLAGS += -DDIFFTEST_PER_SIG
	target = ./obj_dir/V$(NAME)
endif

VERI_INC_DIR = $(OBJ_DIR) $(EMU_DIR)/include include $(EMU_SRC_DIR)
VERI_VFLAGS = --exe $(addprefix -I, $(VERI_INC_DIR)) --top $(NAME)
VERI_CFLAGS = -O3 $(addprefix -I../, $(VERI_INC_DIR)) $(MODE_FLAGS)
VERI_CFLAGS += -DMOD_NAME=S$(NAME) -DREF_NAME=V$(NAME) -DHEADER=\\\"V$(NAME)__Syms.h\\\"
VERI_LDFLAGS = -O3 -lgmp
VERI_VSRCS = $(TEST_FILE).v
VERI_VSRCS += $(addprefix scala/build/, SdCard.v TransExcep.v UpdateCsrs.v UpdateRegs.v InstFinish.v)
VERI_CSRCS = $(shell find $(OBJ_DIR) $(EMU_SRC_DIR) -name "*.cpp") $(EMU_DIR)/difftest-ysyx3.cpp

GSIM_CFLAGS = -O3 $(addprefix -I, $(VERI_INC_DIR)) $(MODE_FLAGS) -DMOD_NAME=S$(NAME)

mainargs = ready-to-run/bin/bbl-hello.bin
# mainargs = ysyx3-bin/rtthread.bin

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(BUILD_DIR)/%.o: %.cxx
	@mkdir -p $(dir $@) && echo + CXX $<
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(TARGET): makedir $(PARSER_BUILD)/syntax.o $(PARSER_BUILD)/lexical.o $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(PARSER_BUILD)/syntax.o $(PARSER_BUILD)/lexical.o -o $(BUILD_DIR)/$(TARGET) -lgmp

makedir:
	mkdir -p build

compile: $(TARGET)
	$(BUILD_DIR)/$(TARGET) $(FIRRTL_FILE)

clean:
	rm -rf obj parser/build obj_dir build

emu: obj/top.cpp $(EMU_SRC) compile
	g++ $(EMU_SRC) obj/top.cpp -DMOD_NAME=S$(NAME) -Wl,-lgmp -Iobj -I$(EMU_SRC_DIR) -o $(BUILD_DIR)/$(EMU_TARGET)
	$(BUILD_DIR)/$(EMU_TARGET)

# flex & bison
$(PARSER_BUILD)/syntax.cc:  $(SYNTAX_SRC)
	bison -v -d $< -o $@

$(PARSER_BUILD)/lexical.cc: $(LEXICAL_SRC)
	flex -o $@ $<

$(PARSER_BUILD)/syntax.o: $(PARSER_BUILD)/syntax.cc
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(PARSER_BUILD)/lexical.o: $(PARSER_BUILD)/lexical.cc
	@$(CXX) $(CXXFLAGS) -c -o $@ $(realpath $<)

$(BUILD_DIR)/S$(NAME): $(VERI_CSRCS)
	$(CXX) $^ $(GSIM_CFLAGS) -lgmp -o $@

./obj_dir/V$(NAME): $(VERI_CSRCS)
	verilator $(VERI_VFLAGS) -Wno-lint -j 8 --cc $(VERI_VSRCS) -CFLAGS "$(VERI_CFLAGS)" -LDFLAGS "$(VERI_LDFLAGS)" $^
	python3 scripts/sigFilter.py
	make -s OPT_FAST="-O3" -j -C ./obj_dir -f V$(NAME).mk V$(NAME)

difftest: $(target)
	$(target) $(mainargs)

count:
	find emu parser src include emu-src scripts -name "*.cpp" -o -name "*.h" -o -name "*.y" -o -name "*.l" -o -name "*.py" |xargs wc

gendoc:
	doxygen
	python3 -m http.server 8080 --directory doc/html

format:
	@clang-format -i --style=file $(SRCS) $(HEADERS)

.PHONY: compile clean emu difftest count makedir gendoc
