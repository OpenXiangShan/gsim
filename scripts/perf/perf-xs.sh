trap 'killall -15 emu' INT
set -x

# get script dir
script_dir=$(realpath $(dirname $0))
gsim_path=$(realpath $script_dir/..)
echo $gsim_path

. $1

numactl -C 0-7 $NOOP_HOME/build/emu --no-diff --ram-size 8GB -i $NOOP_HOME/ready-to-run/linux.bin 2> err.txt &

pid=$!
perf stat -I 1000 -p $pid -e $events
