#!/bin/bash

cd ..
echo "MANY_THREADS_UNBALANCED TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/many_threads_unbalanced' &
    sleep 11
done
