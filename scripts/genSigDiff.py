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
    self.dstFileName = "obj/" + name + "/" + name + "_checkSig"

  def closeDstFile(self):
    if self.dstfp is not None:
      self.dstfp.writelines("return ret;\n}\n")
      self.dstfp.close()

  def newDstFile(self):
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + str(self.fileIdx) + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <gmp.h>\n#include <" + self.name + ".h>\n#include \"top_ref.h\"\n")
    self.dstfp.writelines("bool checkSig" + str(self.fileIdx) + "(bool display, Diff" + self.name + "* ref, S" + self.name + "* mod) {\n")
    self.dstfp.writelines("bool ret = false;\n")
    self.dstfp.writelines("mpz_t tmp1;\nmpz_init(tmp1);\n \
mpz_t tmp2;\nmpz_init(tmp2);\n \
mpz_t tmp3;\nmpz_init(tmp3);\n")
    self.fileIdx += 1
    self.varNum = 0

  def potentialName(self, oldName):
    last_sep_index = oldName.rfind('$')
    ret = oldName
    if last_sep_index != -1:
      ret = oldName[:last_sep_index + 1] + oldName[last_sep_index + 1:].replace('_', '$')
    return ret

  def filter(self, srcFile, refFile):
    self.srcfp = open(srcFile, "r")
    self.reffp = open(refFile, "r")
    all_sigs = {}
    for line in self.reffp.readlines():
      match = re.search(r'((uint[0-9]*_t)|mpz_t)(.*)width =', line)
      if match:
        line = line.strip(" ;\n")
        line = re.split(' |\[|;', line)
        width = line[-1]
        all_sigs[line[1]] = int(width)

    self.newDstFile()

    for line in self.srcfp.readlines():
      line = line.strip("\n")
      line = line.split(" ")
      sign = int(line[0])
      mod_width = int(line[1])
      index = line[3].find('[')
      if index == -1:
        matchName = line[3]
      else:
        matchName = line[3][:index]
      if matchName in all_sigs:
        if mod_width != all_sigs[matchName]:
          continue
        if self.varNum == self.numPerFile:
          self.newDstFile()
        self.varNum += 1
        refName = "ref->" + line[3]
        modName = "mod->" + line[2]

        if mod_width <= 64:
          mask = hex((1 << mod_width) - 1) if mod_width <= 64 else "((uint256_t)" + hex((1 << (mod_width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"

          self.dstfp.writelines( \
          "if(display || (" + modName + " & " + mask + ") != (" + refName + " & " + mask + ")){\n" + \
          "  ret = true;\n" + \
          "  std::cout << std::hex <<\"" + line[2] + ": \" << +" +  \
          (modName if mod_width <= 64 else "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName) + " << \"  \" << +" + \
            (refName if mod_width <= 64 else "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName) + "<< std::endl;\n" + \
          "} \n")
        elif mod_width <= 256 and not sign:
          mask = hex((1 << mod_width) - 1) if mod_width <= 64 else "((uint256_t)" + hex((1 << (mod_width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"
          if mod_width <= 64:
            outMod = modName
            mask = hex((1 << mod_width) - 1)
          elif mod_width <= 128:
            outMod = "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName
            mask = "((uint256_t)" + hex((1 << (mod_width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"
          else:
            outMod = "(uint64_t)(" + modName + " >> 192) <<" + "(uint64_t)(" + modName + " >> 128) <<" + "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName
            mask = "((uint256_t)" + hex((1 << max(0, mod_width - 192))-1) + "<< 192 | " + "(uint256_t)" + hex((1 << min(64, mod_width - 128))-1) + "<< 128 | " + "(uint256_t)" + hex((1 << 64)-1) + "<< 64 | " + hex((1 << 64)-1) + ")"
          if mod_width <= 64:
            outRef = modName
          elif mod_width <= 128:
            outRef = "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName
          else:
            outRef = "(uint64_t)(" + refName + " >> 192) <<" + "(uint64_t)(" + refName + " >> 128) <<" + "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName
          self.dstfp.writelines( \
          "if(display || (" + modName + " & " + mask + ") != (" + refName + " & " + mask + ")){\n" + \
          "  ret = true;\n" + \
          "  std::cout << std::hex <<\"" + line[2] + ": \" << +" +  outMod + " << \"  \" << +" + outRef + "<< std::endl;\n" + \
          "} \n")
        elif mod_width > 256:
          pass

    self.srcfp.close()
    self.reffp.close()
    self.closeDstFile()
    self.dstfp = open(self.dstFileName + ".cpp", "w")
    self.dstfp.writelines("#include <iostream>\n#include <gmp.h>\n#include <" + self.name + ".h>\n#include \"top_ref.h\"\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("bool checkSig" + str(i) + "(bool display, Diff" + self.name + "* ref, S" + self.name + "* mod);\n")
    self.dstfp.writelines("bool checkSig" + "(bool display, Diff" + self.name + "* ref, S" + self.name + "* mod){\nbool ret = false;\n")
    for i in range (self.fileIdx):
      self.dstfp.writelines("ret |= checkSig" + str(i) + "(display, ref, mod);\n")
    self.dstfp.writelines("return ret;\n}\n")
    self.dstfp.close()

if __name__ == "__main__":
  sigFilter = SigFilter(sys.argv[1])
  sigFilter.filter("obj/" + sys.argv[1] + "_sigs.txt", "emu/obj_" + sys.argv[2] + "/top_ref.h")