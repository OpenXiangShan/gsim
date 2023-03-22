BUILD_DIR = ./build
OBJ_DIR = ./obj_dir

test:
	mill -i __.test

verilog:
	mkdir -p $(BUILD_DIR)
	mill -i __.test.runMain addtop -td $(BUILD_DIR) -X high -X low

help:
	mill -i __.test.runMain addtop --help

compile:
	mill -i __.compile

bsp:
	mill -i mill.bsp.BSP/install

reformat:
	mill -i __.reformat

checkformat:
	mill -i __.checkFormat

clean:
	-rm -rf $(BUILD_DIR)
	-rm -rf $(OBJ_DIR)

.PHONY: test verilog help compile bsp reformat checkformat clean 
