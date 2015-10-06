#!/bin/bash

for i in `seq 1 50`;
do
    sh -c './rain ./deadlock' &
    sleep 0.25
    pkill deadlock
done
