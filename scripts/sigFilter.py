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
      idx = ref.find(line[3] + ";")
      sign = int(line[0])
      mod_width = int(line[1])
      if idx != -1:
        ref_width = self.width(idx, ref)
        refName = "ref->rootp->" + line[3]
        if ref_width > 64:
          num = int((ref_width + 31) / 32)
          self.dstfp.writelines("mpz_set_ui(tmp1, ref->rootp->" + line[3] + "[" + str(num-1) +"U]);\n")
          for i in range(num - 1, 0, -1):
            self.dstfp.writelines("mpz_mul_2exp(tmp1, tmp1, 32);\n")
            self.dstfp.writelines("mpz_add_ui(tmp1, tmp1, ref->rootp->" + line[3] + "[" + str(i-1) + "U]);\n")
          refName = "tmp1"
        if sign :
          self.dstfp.writelines("u_asUInt(tmp2, " + "mod->" + line[2] + ", " + line[1] +");\n")
        modName = "tmp2" if sign else "mod->" + line[2]
        if refName == "tmp1" :
          self.dstfp.writelines( \
          "if(display || mpz_cmp(" + modName + ", " + refName + ") != 0){\n" + \
          "  ret = true;\n" + \
          "  std::cout << \"" + line[2] + ": \";\n" + \
          "  mpz_out_str(stdout, 16, mod->" + line[2] + ");\n" \
          "  std::cout << \" \"; mpz_out_str(stdout, 16, " + refName + "); std::cout << std::endl;\n}\n"
          )
        else :
          self.dstfp.writelines( \
          "if(display || mpz_cmp_ui(" + modName + ", " + refName + ") != 0){\n" + \
          "  ret = true;\n" + \
          "  std::cout << \"" + line[2] + ": \";\n" + \
          "  mpz_out_str(stdout, 16, mod->" + line[2] + ");\n" \
          "  std::cout <<\"  \" << std::hex << +" + refName + "<< std::endl;\n" + \
          "} \n")
    self.dstfp.writelines("return ret;\n")
    self.srcfp.close()
    self.reffp.close()
    self.dstfp.close()

if __name__ == "__main__":
  sigFilter = SigFilter()
  sigFilter.filter("obj/allSig.h", "obj_dir/Vnewtop___024root.h", "obj/checkSig.h")