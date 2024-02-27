import sys
import os
import re
from enum import Enum

class Status(Enum):
    start = 1
    struct = 2
    func = 3


class Partition():
    def __init__(self, filePath, outDir, topName):
        self.filePath = filePath
        self.outDir = outDir
        self.topName = topName
        self.srcId = 0
        self.funcPerFile = 100
        self.linePerFile = 10000
        self.funcNum = 0
        self.lineNum = 0
        self.bracketNum = 0
        self.outSrcPath = outDir + '/' + topName + str(self.srcId) + '.cpp'
        self.outFunc = None
        self.newCppFile()

    def toFuc(self, line):
        self.outFunc.write(line)

    def bracketCount(self, line):
        isStr = False
        left = 0
        right = 0
        prevTrans = False
        for char in line:
          if char == '"' and not prevTrans:
            isStr = ~isStr
          elif char == "\\":
            prevTrans = True
          elif char == '{' and not isStr:
            left += 1
          elif char == '}' and not isStr:
            right += 1

          if char != "\\":
            prevTrans = False
        return left - right
    
    def newCppFile(self):
        self.funcNum = 0
        if self.outFunc is not None:
            self.outFunc.close()
        self.outSrcPath = outDir + '/' + self.topName + str(self.srcId) + '.cpp'
        self.outFunc = open(self.outSrcPath, "w")
        self.outFunc.write("#include \"" + self.topName + ".h\"\n")
        self.srcId += 1

    def parseLine(self, line):
        self.lineNum = self.lineNum + 1
        self.outFunc.write(line)
        self.bracketNum += self.bracketCount(line)
        assert (self.bracketNum >= 0)
        if self.bracketNum == 0 and self.lineNum > self.linePerFile:
          self.newCppFile()
          self.lineNum = 0


    def partition(self):
        with open(self.filePath, "r") as f:
            for i, line in enumerate(f):
                self.parseLine(line)
            f.close()

    def __del__(self):
        if self.outFunc is not None:
            self.outFunc.close()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Invalid input")
        exit(-1)

    filePath  = sys.argv[1]
    outDir = sys.argv[2]
    (_, fileName) = os.path.split(filePath)
    (topName, _) = os.path.splitext(fileName)
  
    p = Partition(filePath, outDir, topName)
    p.partition()