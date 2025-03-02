# usage: regression.sh
# See log file at build/$task/$task.log

TASKS="ysyx3 rocket small-boom boom xiangshan xiangshan-default"

cd ..
make clean
make MODE=2 build-gsim -j `nproc`

echo "Regression start at $(date)"
for t in $TASKS; do
  ((make diff dutName=$t -j `nproc` > /dev/null);
    date | xargs -I '{}' echo '{}' ": finish $t") &
done
