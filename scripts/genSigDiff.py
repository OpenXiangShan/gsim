import sys
import os
class SigFilter():
  def __init__(self):
    self.srcfp = None
    self.reffp = None
    self.dstfp = None

  def width(self, idx, data):
    endIdx = idx
    while data[endIdx] != ':':
      endIdx -= 1
    startIdx = endIdx
    while data[startIdx-1] != '*':
      startIdx -= 1
    return int(data[startIdx : endIdx]) + 1

  def filter(self, srcFile, refFile, dstFile):
    self.srcfp = open(srcFile, "r")
    self.reffp = open(refFile, "r")
    self.dstfp = open(dstFile, "w")
    ref = self.reffp.read()
    self.dstfp.writelines("bool ret = false;\n")
    self.dstfp.writelines("mpz_t tmp1;\nmpz_init(tmp1);\n \
mpz_t tmp2;\nmpz_init(tmp2);\n \
mpz_t tmp3;\nmpz_init(tmp3);\n")
    for line in self.srcfp.readlines():
      line = line.strip("\n")
      line = line.split(" ")
      sign = int(line[0])
      width = int(line[1])
      name = line[2]
      splitReg = line[4]
      if len(name) > 5 and name[-5:] == "$prev":
        name = name[0:len(name)-5]
        if splitReg == '1':
          name = name + "$next"
      idx = ref.find(" " + name + ";")
      modName = "mod->" + name
      refName = "ref->" + name
      if idx != -1:
        if width <= 64:
          mask = hex((1 << width) - 1) if width <= 64 else "((__uint128_t)" + hex((1 << (width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"

          self.dstfp.writelines( \
          "if(display || (" + modName + " & " + mask + ") != (" + refName + " & " + mask + ")){\n" + \
          "  ret = true;\n" + \
          "  std::cout << std::hex <<\"" + name + ": \" << +" +  \
          (modName if width <= 64 else "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName) + " << \"  \" << +" + \
            (refName if width <= 64 else "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName) + "<< std::endl;\n" + \
          "} \n")
        elif width <= 128 and not sign:
          mask = hex((1 << width) - 1) if width <= 64 else "((__uint128_t)" + hex((1 << (width - 64))-1) + "<< 64 | " + hex((1 << 64)-1) + ")"
          self.dstfp.writelines( \
          "if(display || (" + modName + " & " + mask + ") != (" + refName + " & " + mask + ")){\n" + \
          "  ret = true;\n" + \
          "  std::cout << std::hex <<\"" + name + ": \" << +" +  \
          (modName if width <= 64 else "(uint64_t)(" + modName + " >> 64) << " + "(uint64_t)" + modName) + " << \"  \" << +" + \
            (refName if width <= 64 else "(uint64_t)(" + refName + " >> 64) << " + "(uint64_t)" + refName) + "<< std::endl;\n" + \
          "} \n")
    self.dstfp.writelines("return ret;\n")
    self.srcfp.close()
    self.reffp.close()
    self.dstfp.close()

if __name__ == "__main__":
  sigFilter = SigFilter()
  sigFilter.filter("obj/" + sys.argv[1] + "_sigs.txt", "emu/obj_" + sys.argv[2] + "/top_ref.h", "obj/checkSig.h")