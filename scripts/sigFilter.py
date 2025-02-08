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
    self.dstFileName = sys.argv[1] + "/" + name + "-split/" + name + "_checkSig"

  def closeDstFile(self):
    if self.dstfp is not None:
      self.dstfp.writelines("return ret;\n}\n")
      self.dstfp.close()

  def newDstFile(self):
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + str(self.fileIdx) + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <gmp.h>\n#include <" + self.name + ".h>\n#include \"V" + self.name + "__Syms.h\"\n")
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
        refUName = line[3] + "_u"
        if mod_width > 256:
          continue
        if mod_width > 128 and mod_width <= 256:
          num = int((ref_width + 31) / 32)
          self.dstfp.writelines("uint256_t " + refUName + " = " + refName + "[" + str(num-1) +"U];\n")
          for i in range(num - 1, 0, -1):
            self.dstfp.writelines(refUName + " = (" + refUName + " << 32) + " + refName + "[" + str(i-1) + "U];\n")

        refName128 = ""
        if ref_width > 64:
          num = int((ref_width + 31) / 32)
          for i in range(min(8, num)):
            refName128 = refName128 + (" | " if i != 0 else "") + "((uint256_t)" + refName + "[" + str(i) + "] << " + str(i * 32) + ")"
          refName = refName.lstrip("ref->rootp->") + "_128"
          self.dstfp.writelines("uint256_t " + refName + " = " + refName128 + ";\n")
        # mask = hex((1 << mod_width) - 1) if mod_width <= 64 else "((uint256_t)" + hex((1 << (mod_width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"
        mask = "(((uint256_t)1 << " + str(mod_width) + ") - 1)"
        self.dstfp.writelines( \
        "if(display || (" + modName + " & " + mask + ") != (" + refName + "&" + mask + ")){\n" + \
        "  ret = true;\n" + \
        "  std::cout << std::hex <<\"" + line[2] + ": \" << +" +  \
        (modName if mod_width <= 64 else "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName) + " << \"  \" << +" + \
          (refName if ref_width <= 64 else "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName) + "<< std::endl;\n" + \
        "} \n")
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
  sigFilter.filter(sys.argv[1] + "/" + sys.argv[2] + "_sigs.txt", "obj_dir/V" + sys.argv[2] + "___024root.h")
