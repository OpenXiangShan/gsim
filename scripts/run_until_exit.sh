#!/usr/bin/env bash

CMD=("${@:1}")

echo "Running "${CMD[@]}""
coproc ("${CMD[@]}")
pid=${COPROC_PID}
while read -r o <&"${COPROC[0]}"
do
    echo "${o}"
    if [[ "$o" =~ "Exit with code" ]]; then
        kill $pid
        wait $pid
        if [[ "$o" =~ "0" ]]; then
            exit 0
        else
            exit 1
        fi
    fi
    if [[ "$o" =~ "MicroBench PASS" ]]; then
        kill $pid
        wait $pid
        exit 0
    fi
done
exit 1
