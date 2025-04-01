#/bin/bash

STEP=5
MAX=100

TASKS="ysyx3 rocket small-boom large-boom minimal-xiangshan default-xiangshan"

function log() {
  echo "`date`|| $1"
}

function rebuild_gsim() {
  make clean
  make $1 build-gsim -j `nproc` > /dev/null
  if [ $? != 0 ] ; then exit; fi
}

cd ..

rebuild_gsim MODE=0

for t in $TASKS; do
  logfile=build/$t-supernode.txt
  > $logfile

  log "exploring(coarse-grain) $t..."
  for i in `seq $STEP $STEP $MAX`; do
    rm -rf build/$t/model
    make run dutName=$t GSIM_FLAGS_EXTRA="--supernode-max-size=$i" -j `nproc` > /dev/null
    result=`grep "simulation process 100.00%" build/$t/$t.log | grep -o "[0-9]* per sec" | awk '{print $1}'`
    log "$t(--supernode-max-size=$i): ${result}Hz"
    echo $i ${result} >> $logfile
  done

  log "exploring(fine-grain) $t..."
  max=`sort -nr -k 2 $logfile | head -n 1 | awk '{print $1}'`
  left=$(($max - $STEP + 1))
  right=$(($max + $STEP - 1))
  for i in `seq $left $right`; do
    rm -rf build/$t/model
    make run dutName=$t GSIM_FLAGS_EXTRA="--supernode-max-size=$i" -j `nproc` > /dev/null
    result=`grep "simulation process 100.00%" build/$t/$t.log | grep -o "[0-9]* per sec" | awk '{print $1}'`
    log "$t(--supernode-max-size=$i): ${result}Hz"
    echo $i ${result} >> $logfile
  done

  max=`sort -nr -k 2 $logfile | head -n 1 | awk '{print $1}'`
  result=`sort -nr -k 2 $logfile | head -n 1 | awk '{print $2}'`
  log "--supernode-max-size=$max (${result}HZ) is the best for $t"
done
