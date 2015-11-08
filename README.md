# rain
pthreads profiler. Measure load balance, lock contention and detect deadlocks.

My senior project. Currently implementing various tests for existing features. Additionally, I would like to continue wrapping pthreads methods/features to increase compatibility with existing programs that utilize pthreads.

librain.cpp contains the profiling code. 
Example runs:  
./rain ./tests/two_threads_unbalanced 4 7  
./rain ./tests/many_threads_unbalanced  
./rain ./tests/deadlock  
./rain ./tests/nodeadlock  
