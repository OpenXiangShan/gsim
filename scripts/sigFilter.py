import sys
import os
import re

class SigFilter():
  def __init__(self, name):
    self.srcfp = None
    self.reffp = None
    self.dstfp = None
    self.name = name
    self.numPerFile = 10000
    self.fileIdx = 0
    self.varNum = 0
    self.dstFileName = sys.argv[1] + "/model/" + name + "_checkSig"

  def closeDstFile(self):
    if self.dstfp is not None:
      self.dstfp.writelines("return ret;\n}\n")
      self.dstfp.close()

  def newDstFile(self):
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + str(self.fileIdx) + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <" + self.name + ".h>\n#include \"V" + self.name + "__Syms.h\"\n")
    self.dstfp.writelines("bool checkSig" + str(self.fileIdx) + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod) {\n")
    self.dstfp.writelines("bool ret = false;\n")
    self.fileIdx += 1
    self.varNum = 0

  def width(self, data):
    endIdx = len(data) - 1
    while data[endIdx] != ':':
      endIdx -= 1
    startIdx = endIdx
    while data[startIdx-1] != '*':
      startIdx -= 1
    return int(data[startIdx : endIdx]) + 1

  def genDiffCode(self, modName, refName, line, mod_width, ref_width):
    refUName = line[3] + "_u"
    if mod_width > 128:
      num = int((ref_width + 31) / 32)
      self.dstfp.writelines("unsigned _BitInt(" + str(num*32) + ") " + refUName + " = " + refName + "[" + str(num-1) +"U];\n")
      for i in range(num - 1, 0, -1):
        self.dstfp.writelines(refUName + " = (" + refUName + " << 32) + " + refName + "[" + str(i-1) + "U];\n")

    refName128 = ""
    if ref_width > 64:
      num = int((ref_width + 31) / 32)
      bits = num * 32
      utype = "unsigned _BitInt(" + str(bits) + ")"
      for i in range(num):
        refName128 = refName128 + (" | " if i != 0 else "") + "((" + utype + ")" + refName + "[" + str(i) + "] << " + str(i * 32) + ")"
      refName = refName.lstrip("ref->rootp->") + "_" + str(bits)
      self.dstfp.writelines(utype + " " + refName + " = " + refName128 + ";\n")
    mask = (hex((1 << mod_width) - 1) + "u" if mod_width <= 64 else
            "((unsigned _BitInt(" + str(mod_width) + "))0 - 1)")
    self.dstfp.writelines("if( display || (((" + modName + " ^ " + refName + ") & " + mask + ") != 0)) {\n" + \
                          "  ret = true;\n" + \
                          "  std::cout << std::hex << \"" + line[2] + ": \" ")
    num = int((mod_width + 63) / 64)
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + modName + " >> " + str(i * 64) + ") << '_'")
    self.dstfp.writelines(" << \"  \" ")
    num = int((ref_width + 63) / 64)
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + refName + " >> " + str(i * 64) + ") << '_'")
    self.dstfp.writelines(" << std::endl;\n" + "} \n")

  def genDiffCode2(self, modName, refName, line, width):
    # This code is simpler, but it does not work with clang 16.
    # clang 16 can not correctly handle pointer of _BitInt like:
    #     *(_BitInt(28) *)a
    # It should work with clang 19.
    utype = "unsigned _BitInt(" + str(width) + ")"
    modNameLocal = line[3] + "_dut"
    refNameLocal = line[3] + "_ref"
    self.dstfp.writelines(utype + " " + modNameLocal + " = " + "*(" + utype + "*)(&(" + modName + "));\n")
    self.dstfp.writelines(utype + " " + refNameLocal + " = " + "*(" + utype + "*)(&(" + refName + "));\n")
    self.dstfp.writelines("if (display || (" + modNameLocal + " != " + refNameLocal + ")) {\n" + \
                          "  ret = true;\n" + \
                          "  std::cout << std::hex <<\"" + line[2] + ": \" ")
    num = int((width + 63) / 64)
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + modNameLocal + " >> " + str(i * 64) + ")")
    self.dstfp.writelines(" << \"  \" ")
    for i in range(num - 1, -1, -1):
      self.dstfp.writelines(" << (uint64_t)(" + refNameLocal + " >> " + str(i * 64) + ")")
    self.dstfp.writelines(" << std::endl;\n" + "} \n")

  def filter(self, srcFile, refFile):
    self.srcfp = open(srcFile, "r")
    self.reffp = open(refFile, "r")
    # self.dstfp = open(dstFile, "w")
    all_sigs = {}
    for line in self.reffp.readlines():
      match = re.search(r'/\*[0-9]*:[0-9]*\*/ ', line)
      if match:
        line = line.strip(" ;\n")
        line = line.split(" ")
        all_sigs[line[len(line) - 1]] = self.width(line[0])

    self.newDstFile()

    for line in self.srcfp.readlines():
      line = line.strip("\n")
      line = line.split(" ")
      sign = int(line[0])
      mod_width = int(line[1])
      if line[3] in all_sigs:
        if self.varNum == self.numPerFile:
          self.newDstFile()
        self.varNum += 1
        ref_width = all_sigs[line[3]]
        refName = "ref->rootp->" + line[3]
        modName = "mod->" + line[2]
        if mod_width > ref_width:
          continue
        assert(mod_width <= ref_width), "width(" + line[2] + ") = " + str(mod_width) + ", width(" + line[3] + ") = " + str(ref_width)
        width = mod_width

        self.genDiffCode(modName, refName, line, mod_width, ref_width)
        #self.genDiffCode2(modName, refName, line, width)

    # self.dstfp.writelines("return ret;\n")
    self.srcfp.close()
    self.reffp.close()
    # self.dstfp.close()
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <" + self.name + ".h>\n#include \"V" + self.name + "__Syms.h\"\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("bool checkSig" + str(i) + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod);\n")
    self.dstfp.writelines("bool checkSig" + "(bool display, V" + self.name + "* ref, S" + self.name + "* mod){\nbool ret = false;\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("ret |= checkSig" + str(i) + "(display, ref, mod);\n")
    self.dstfp.writelines("return ret;\n}\n")
    self.dstfp.close()


if __name__ == "__main__":
  sigFilter = SigFilter(sys.argv[2])
  sigFilter.filter(sys.argv[1] + "/model/" + sys.argv[2] + "_sigs.txt",
                   sys.argv[1] + "/verilator/V" + sys.argv[2] + "___024root.h")
