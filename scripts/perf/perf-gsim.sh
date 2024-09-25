trap 'killall -15 SSimTop' INT
set -x

# get script dir
script_dir=$(realpath $(dirname $0))
gsim_path=$(realpath $script_dir/../..)
echo $gsim_path

. $1

numactl -C 0-7 $gsim_path/build/emu/SSimTop $gsim_path/ready-to-run/bin/linux-xiangshan.bin &

pid=$!
perf stat -I 1000 -p $pid -e $events
