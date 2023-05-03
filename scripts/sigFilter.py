import sys
import os
class SigFilter():
  def __init__(self):
    self.srcfp = None
    self.reffp = None
    self.dstfp = None

  def filter(self, srcFile, refFile, dstFile):
    self.srcfp = open(srcFile, "r")
    self.reffp = open(refFile, "r")
    self.dstfp = open(dstFile, "w")
    ref = self.reffp.read()
    self.dstfp.writelines("bool ret = false;\n")
    for line in self.srcfp.readlines():
      line = line.strip("\n")
      line = line.split(" ")
      if ref.find(line[1] + ";") != -1:
        self.dstfp.writelines( \
        "if(display || mpz_cmp_ui(mod->" + line[0] + ", ref->rootp->" + line[1] + ") != 0){\n" + \
        "  ret = true;\n" + \
        "  std::cout << \"" + line[0] + ": \"; mpz_out_str(stdout, 16, mod->" + line[0] + "); std::cout <<\"  \" << std::hex << ref->rootp->" + line[1] + "<< std::endl;\n" + \
        "} \n")
    self.dstfp.writelines("return ret;\n")
    self.srcfp.close()
    self.reffp.close()
    self.dstfp.close()

if __name__ == "__main__":
  sigFilter = SigFilter()
  sigFilter.filter("obj/allSig.h", "obj_dir/Vtop___024root.h", "obj/checkSig.h")