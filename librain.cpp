#include <pthread.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <ucontext.h>
#include <errno.h>

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <atomic>   // pthreads has no atomic support, so this is
                    // used for initializing pthreads library functions

typedef struct RegSet {
    // use signal context to read register set
    greg_t generalRegister[NGREG];
} RegSet;

static unsigned int activeThreadCount = 0;      // how many threads are actually running
static unsigned int totalThreadCount = 0;       // total threads that ran at once (max active)
static std::vector<pthread_t *> clientThreads;  // access to client's threads
static std::vector<sig_atomic_t> sigCounts;     // count of signals thread receives
static std::vector<RegSet> lastRegSet;          // set of registers for each thread at last profile interval

// data structures maintained for deadlock detection
static std::unordered_map<pthread_mutex_t *, int> lockHolders;          // map of mutexes to the thread id holding the mutex lock
static std::unordered_map<int, pthread_mutex_t *> threadWaiters;        // map of thread ids to the mutex it is waiting to acquire (if any)

typedef struct MutexData {
    int lockCount;
    int contention;

    uint64_t lockTimeStamp;
    uint64_t lockTimeTotal;
} MutexData;

// map mutexes to count of times they were already held when another thread tried to acquire them
static std::unordered_map<pthread_mutex_t *, MutexData> mutexData;

// per-thread flag to disable some aspects of profiler
static __thread bool disableRain = false;

static std::atomic_flag real_pthread_initialized = ATOMIC_FLAG_INIT;    // pthread functions initialized?
static pthread_mutex_t rain_lock = PTHREAD_MUTEX_INITIALIZER;         // lock for internal operations

// need to be able to call the actual pthread library functions
static int (*real_pthread_create)(pthread_t*, const pthread_attr_t*, 
            void *(*)(void*), void*) = NULL;
static int (*real_pthread_join)(pthread_t, void **) = NULL;
static void (*real_pthread_exit)(void *) __attribute__((noreturn)) = NULL;
static int (*real_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;

#define WRAP_FUNCTION(FUN_NAME)                                                         \
    do {                                                                                \
        /* c++ doesn't support casting void pointers to function pointers, workaround:*/\
        /* http://stackoverflow.com/questions/1096341/function-pointers-casting-in-c*/  \
        static_assert(sizeof(real_##FUN_NAME), "pointer cast impossible");              \
        *reinterpret_cast<void**>(&real_##FUN_NAME) = dlsym(RTLD_NEXT, #FUN_NAME);      \
        if (real_##FUN_NAME == NULL) {                                                  \
            fprintf(stderr, "ERROR: RAIN: #FUN_NAME, dlsym: %s\n", dlerror());          \
        }                                                                               \
    } while (0)

static uint64_t timeNow() {
    struct timespec t;
    int res = clock_gettime(CLOCK_MONOTONIC, &t);
    if (res) {
        fprintf(stderr, "ERROR: RAIN: clock_gettime, %d\n", res);
        return 0;
    }
    return t.tv_sec * 1000000000 + t.tv_nsec;
}

static void printBacktrace() {
    /* glibc specific backtrace functions for stack traces
     * backtrace_symbols requires client programs to be compiled with -rdynamic
     * however, if they aren't it just won't give function names and still works gracefully
     */

    /* backtrace uses a mutex, so we need to disable some wrappers temporarily */
    disableRain = true;

    /* gnu.org: 200 possible entries should probably cover all programs */
    static const int NUM_RET_ADDR = 200;
    void *callstack[NUM_RET_ADDR];
    int i, frames = backtrace(callstack, NUM_RET_ADDR);
    char **strs = backtrace_symbols(callstack, frames);
    for (i = 0; i < frames; ++i) {
        printf("%s\n", strs[i]);
    }
    disableRain = false;
}

static void init_real_pthreads() {
    // initialize real pthread function pointers
    WRAP_FUNCTION(pthread_create);
    WRAP_FUNCTION(pthread_join);
    WRAP_FUNCTION(pthread_exit);
    WRAP_FUNCTION(pthread_mutex_lock);
    WRAP_FUNCTION(pthread_mutex_unlock);

    // gcc 4.9 has a bug with unordered_map that causes a floating point exception
    // reserve here as a workaround to prevent it
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=61143
    lockHolders.reserve(1);
    threadWaiters.reserve(1);
    mutexData.reserve(1);
}

static void sigprof_handler(int sig_nr, siginfo_t* info, void *context) {
    (void)sig_nr;
    (void)info;
    (void)context;
    /*
    // to block SIGPROF while handling it:
    sigset_t block_set;
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGPROF);
    pthread_sigmask(SIG_BLOCK, &block_set, NULL);
    //sigprocmask(SIG_BLOCK, &block_set, NULL);
    */
    unsigned int t;
    for (t = 0; t < totalThreadCount; ++t) {
        sigval sval;
        sval.sival_int = t;
        // send SIGUSR1 to each thread so they can all grab a call stack
        if (clientThreads[t]) {
            // pthread_sigqueue is GNU specific, but allows
            // passing an integer value to the signal handler
            int res = pthread_sigqueue(*clientThreads[t], SIGUSR1, sval);
            if (res) {
                // can be used for extra debugging, however can get flooded with this error because it takes a few
                // instructions between the actual calls to pthread_create/pthread_join and updating my data structures
                //fprintf(stderr, "ERROR: RAIN: sigprof_handler: pthread_sigqueue: SIGUSR1 signal not sent: %d, %d\n", t, res);
            }
            //pthread_kill(*clientThreads[t], SIGUSR1);
        }
    }
    //pthread_sigmask(SIG_UNBLOCK, &block_set, NULL);
}

static void sigusr1_handler(int sig_nr, siginfo_t* info, void *context) {
    (void)sig_nr;
    unsigned int t = info->si_value.sival_int;

    // instead of just counting signals received by a thread, also check
    // current register set to see if it has changed from last check
    // (so we can get some idea of whether or not thread has actually done work)
    bool regChanged = false;
    ucontext_t *ucontext = (ucontext_t*)context;
    for (int i = 0; i < NGREG; i++) {
        if (ucontext->uc_mcontext.gregs[i] != lastRegSet[t].generalRegister[i]) {
            regChanged = true;
        }
        lastRegSet[t].generalRegister[i] = ucontext->uc_mcontext.gregs[i];
    }
    if (regChanged) {
        ++sigCounts[t];
    }
}

// called just before first thread is created
static void begin() {
    totalThreadCount = 0;
    clientThreads.clear();
    sigCounts.clear();
    lastRegSet.clear();

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigprof_handler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGPROF, &sa, NULL);

    struct sigaction sa2;
    memset(&sa2, 0, sizeof(sa2));
    sa2.sa_sigaction = sigusr1_handler;
    sa2.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa2.sa_mask);
    sigaction(SIGUSR1, &sa2, NULL);

    static struct itimerval _RAIN_timer;
    _RAIN_timer.it_interval.tv_sec = 0;
    _RAIN_timer.it_interval.tv_usec = 1000000 / 100; /* 100hz */
    _RAIN_timer.it_value = _RAIN_timer.it_interval;
    if (setitimer(ITIMER_PROF, &_RAIN_timer, NULL)) {
        fprintf(stderr, "ERROR: RAIN: begin: timer could not be initialized: %s\n",
                strerror(errno));
    }
}

// called just after last thread joined
static void finish() {
    struct itimerval _RAIN_timer = {0};
    if (setitimer(ITIMER_PROF, &_RAIN_timer, NULL)) {
        fprintf(stderr, "ERROR: RAIN: finish: timer could not be stopped: %s\n",
                strerror(errno));
    }

    for (unsigned int t = 0; t < sigCounts.size(); t++) {
        printf("thread %d sigcount %d\n", t, sigCounts[t]);
    }
    if (mutexData.size()) {
        printf("%lu mutexes used\n", mutexData.size());
        printf("Mutex\t\tLocked\tContention\tTotal Time (ms)\n");
        for (auto kv : mutexData) {
            printf("%p\t%d\t%d\t\t%.4f\n", kv.first, kv.second.lockCount, kv.second.contention, kv.second.lockTimeTotal / 1000000.0);
        }
    }
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine) (void *), void *arg) {

    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    // lock this all up in case of threads creating threads
    int res = real_pthread_mutex_lock(&rain_lock);
    if (res) {
        // if unable to lock, try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_create: could not lock thread: %d\n", res);
        return real_pthread_create(thread, attr, start_routine, arg);
    }
    if (activeThreadCount++ == 0) {
        begin();  // first thread, initialize
    }
    int ret = real_pthread_create(thread, attr, start_routine, arg);
    clientThreads.push_back(thread);
    ++totalThreadCount;

    RegSet initRegSet;
    lastRegSet.push_back(initRegSet);
    sigCounts.push_back(0);

    res = real_pthread_mutex_unlock(&rain_lock);
    if (res) {
        // looking at man pages, this would mean rain_lock did not own this lock somehow
        // (otherwise the error would have happened above at real_pthread_mutex_lock)
        fprintf(stderr, "ERROR: RAIN: pthread_create: could not unlock thread: %d\n", res);
    }
    return ret;
}

int pthread_join(pthread_t thread, void **value_ptr) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    int ret = real_pthread_join(thread, value_ptr);
    int res = real_pthread_mutex_lock(&rain_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_join: could not lock thread: %d\n", res);
        return ret;
    }
    unsigned int t;
    for (t = 0; t < totalThreadCount; ++t) {
        if (clientThreads[t] && pthread_equal(*clientThreads[t], thread)) {
            clientThreads[t] = 0;
            break;  // found the thread
        }
    }
    if (--activeThreadCount == 0) {
        finish();    // last thread, cleanup, output traces
    }
    res = real_pthread_mutex_unlock(&rain_lock);
    if (res) {
        // likely means rain_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_join: could not unlock thread: %d\n", res);
    }
    return ret;
}

void pthread_exit(void *value_ptr) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    real_pthread_exit(value_ptr);
}

static bool deadlockDetectRecur(int threadRequesting, pthread_mutex_t *mutex) {
    if (lockHolders.count(mutex) == 0) {
        return 0;   // no thread currently holding this lock
    }
    int holder = lockHolders[mutex];
    if (holder < 0) {
        return 0;   // no thread currently holding this lock
    }
    if (holder == threadRequesting) {
        return 1;   // cycle detected
    }

    if (threadWaiters.count(holder) == 0) {
        return 0;   // holder is not waiting on any locks
    }
    return deadlockDetectRecur(threadRequesting, threadWaiters[holder]);
}

// returns 1 if given thread trying to acquire given mutex creates a deadlock, else 0
static bool deadlockDetect(int thread, pthread_mutex_t *mutex) {
    return deadlockDetectRecur(thread, mutex);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    if (disableRain) {
        return real_pthread_mutex_lock(mutex);
    }
    int res = real_pthread_mutex_lock(&rain_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not lock thread: %d\n", res);
        return real_pthread_mutex_lock(mutex);
    }
    bool found = false;
    unsigned int t;
    for (t = 0; !found && t < totalThreadCount; ++t) {
        if (clientThreads[t] && pthread_equal(*clientThreads[t], pthread_self())) {
            found = true;
        }
    }

    if (!found) {
        // commented out to prevent some programs from flooding this error
        //fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: requesting thread not found, %p\n");
        res = real_pthread_mutex_unlock(&rain_lock);
        if (res) {
            // likely means mutex_lock does not own the mutex somehow
            fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not unlock thread: %d\n", res);
        }
        return real_pthread_mutex_lock(mutex);
    }

    int ret = 0;
    if (!pthread_mutex_trylock(mutex)) {
        // mutex acquired
        lockHolders[mutex] = t;
        ++mutexData[mutex].lockCount;
        mutexData[mutex].lockTimeStamp = timeNow();
    } else {
        // mutex not acquired
        ++mutexData[mutex].contention;
        threadWaiters[t] = mutex;
        if (deadlockDetect(t, mutex)) {
            // TODO slight chance that a deadlock will not actually occur,
            // possible to check here, but it is released before the actual lock attempt
            // although, the deadlock chance still exists, so not necessarily bad to print error
            printf("DEADLOCK DETECTED\n");
            printf("mutex %p, thread %d, %p\n", mutex, t, clientThreads[t]);
            printBacktrace();
        }
        res = real_pthread_mutex_unlock(&rain_lock);
        if (res) {
            // likely means mutex_lock does not own the mutex somehow
            fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not unlock thread: %d\n", res);
        }
        ret = real_pthread_mutex_lock(mutex);
        res = real_pthread_mutex_lock(&rain_lock);
        if (res) {
            // if unable to lock, just try not to disrupt host program
            fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not lock thread: %d\n", res);
            return ret;
        }
        lockHolders[mutex] = t;
        ++mutexData[mutex].lockCount;
        mutexData[mutex].lockTimeStamp = timeNow();
        threadWaiters.erase(t);
    }
    res = real_pthread_mutex_unlock(&rain_lock);
    if (res) {
        // likely means mutex_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not unlock thread: %d\n", res);
    }
    return ret;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    if (disableRain) {
        return real_pthread_mutex_unlock(mutex);
    }
    int res = real_pthread_mutex_lock(&rain_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_unlock: could not lock thread: %d\n", res);
        return real_pthread_mutex_unlock(mutex);
    }
    int ret = real_pthread_mutex_unlock(mutex);
    lockHolders[mutex] = -1;
    mutexData[mutex].lockTimeTotal += timeNow() - mutexData[mutex].lockTimeStamp;
    real_pthread_mutex_unlock(&rain_lock);
    if (res) {
        // likely means mutex_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_unlock: could not unlock thread: %d\n", res);
    }
    return ret;
}

