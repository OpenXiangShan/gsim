BUILD_DIR = ./build
SRCS = $(shell find src -name "*.cpp")

CXXFLAGS = -O2 -I include -DOBJ_DIR=\"obj\"
CXX = g++
TARGET = sim

LEXICAL_SRC = ./parser/lexical.l
SYNTAX_SRC = ./parser/syntax.y

run:
	mkdir -p build
	mkdir -p obj
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(BUILD_DIR)/$(TARGET)
	$(BUILD_DIR)/$(TARGET)

clean:
	rm -rf obj

parser:
	flex $(LEXICAL_SRC)
	bison -v -d $(SYNTAX_SRC) -Wcounterexamples

.PHONY: run clean parser