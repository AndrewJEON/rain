# rain
pthreads profiler. Measure load balance, lock contention and detect deadlocks.

My senior project. Currently still working on implementing deadlock detection and lock contention. Additionally, I would like to continue wrapping pthreads methods/features to increase compatibility with existing programs that utilize pthreads.

librain.cpp contains the profiling code. 
Example runs:  
./rain ./inbalanced_sum 4 7  
./rain ./many_threads_unbalanced  
./rain ./deadlock  
./rain ./nodeadlock  
