#!/bin/bash

cd ..
echo "INBALANCED_SUM TESTS:"
for i in `seq 1 50`;
do
    sh -c './rain ./tests/inbalanced_sum 2 2'
done
