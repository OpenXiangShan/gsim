#!/bin/bash

# usage: regression.sh
# See log file at build/$task/$task.log and build/regression.log

TASKS="ysyx3 rocket small-boom large-boom minimal-xiangshan default-xiangshan"

function log() {
  echo "`date`|| $1" | tee -a build/regression.log
}

function rebuild_gsim() {
  make clean
  make $1 build-gsim -j `nproc` > /dev/null
  if [ $? != 0 ] ; then exit; fi
}

cd ..

rebuild_gsim MODE=2
log "Functional regression start"
(for t in $TASKS; do
  (make diff dutName=$t -j `nproc` > /dev/null;
   result="fail";
   if grep "simulation process 100.00%" build/$t/$t.log > /dev/null; then result="pass"; fi;
   log "$t: ${result}"
  ) &
done; wait) & all1=$!
wait $all1
log "Functional regression finish"


rebuild_gsim MODE=0
log "Performance regression start"
for t in $TASKS; do
  make run dutName=$t -j `nproc` > /dev/null
  result=`grep "simulation process 100.00%" build/$t/$t.log | grep -o "[0-9]* per sec" | awk '{print $1}'`
  make pgo dutName=$t -j `nproc` > /dev/null
  result_pgo=`grep "simulation process 100.00%" build/$t/$t.log | grep -o "[0-9]* per sec" | awk '{print $1}'`
  make bolt dutName=$t -j `nproc` > /dev/null
  result_bolt=`grep "simulation process 100.00%" build/$t/$t.log | grep -o "[0-9]* per sec" | awk '{print $1}'`
  log "$t(plain / pgo / bolt ): ${result}Hz / ${result_pgo}Hz / ${result_bolt}Hz"
done
log "Performance regression finish"
