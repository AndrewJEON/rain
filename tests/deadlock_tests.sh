#!/bin/bash

cd ..
echo "DEADLOCK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/deadlock' &
    sleep 0.2
    pkill deadlock
done

sleep 0.2
echo "DEADLOCK2 TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/deadlock2' &
    sleep 0.2
    pkill deadlock
done

sleep 0.2
echo "SELF_DEADLOCK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/self_deadlock' &
    sleep 0.2
    pkill deadlock
done

sleep 0.2
echo "NO_DEADLOCK TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/nodeadlock' &
    sleep 0.2
done
