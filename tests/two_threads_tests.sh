#!/bin/bash

cd ..
echo "TWO_THREADS_UNBALANCED TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/two_threads_unbalanced 2 2'
done
