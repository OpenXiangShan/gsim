tests=(
  "ysyx3"
  "NutShell"
  "rocket"
  "boom"
  "small-boom"
  "xiangshan"
  "xiangshan-default"
)
tests_num=${#tests[*]}

for ((i=0; i < ${tests_num}; i ++))
do
  testName=${tests[i]}
  make compile -j dutName=${testName} PERF=1 > logs/log-${testName}
  make difftest -j dutName=${testName} PERF=1 > logs/emu-${testName}
done