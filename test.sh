#!/bin/bash

echo "DEADLCOK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./deadlock' &
    sleep 0.2
    pkill deadlock
done

echo "DEADLOCK2 TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./deadlock2' &
    sleep 0.2
    pkill deadlock
done

echo "SELF_DEADLOCK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./self_deadlock' &
    sleep 0.2
    pkill deadlock
done

echo "NO_DEADLOCK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./nodeadlock' &
    sleep 0.2
done
