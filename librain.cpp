#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
//static std::vector<std::string> threadStacks;   // storage for stack traces
static std::vector<sig_atomic_t> sigCounts;     // count of signals thread receives
static std::vector<RegSet> lastRegSet;          // set of registers for each thread at last profile interval

// data structures maintained for deadlock detection
static std::unordered_map<pthread_mutex_t *, int> lockHolders;          // map of mutexes to the thread id holding the mutex lock
static std::unordered_map<int, pthread_mutex_t *> threadWaiters;        // map of thread ids to the mutex it is waiting to acquire (if any)

// map mutexes to count of times they were already held when another thread tried to acquire them
static std::unordered_map<pthread_mutex_t *, int> contention;

static std::atomic_flag real_pthread_initialized = ATOMIC_FLAG_INIT;    // pthread functions initialized?
static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;         // lock for thread create/join
static pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;          // lock for mutex lock/unlock

static void sigprof_handler(int sig_nr, siginfo_t* info, void *context) {
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
    unsigned int t = info->si_value.sival_int;

    /*
    // TODO: this is temporary, gcc backtrace stuff needs the -rdynamic
    // compile flag, which means client programs have to be recompiled and
    // there are issues with thread/signal safety I don't know if I can get around
    static const int NUM_RET_ADDR = 200;    // gnu.org: 200 possible entries should
                                            // probably cover all programs
    void *callstack[NUM_RET_ADDR];
    int i, frames = backtrace(callstack, NUM_RET_ADDR);
    char **strs = backtrace_symbols(callstack, frames);
    std::string trace = "\n";
    for (i = 0; i < frames; ++i) {
        trace += strs[i] + std::string("\n");
    }
    threadStacks[t] += trace;
    */

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
    //threadStacks.clear();
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

    /*
    FILE *fp;
    std::string fileName;
    for (unsigned int t = 0; t < threadStacks.size(); t++) {
        fileName = "thread" + std::to_string(t) + "_traces";
        fp = fopen(fileName.c_str(), "w");
        fprintf(fp, threadStacks[t].c_str());
        fclose(fp);
    }
    */
}

// need to be able to call the actual pthread library functions
static int (*real_pthread_create)(pthread_t*, const pthread_attr_t*, 
            void *(*)(void*), void*) = NULL;
static int (*real_pthread_join)(pthread_t, void **) = NULL;
static void (*real_pthread_exit)(void *) = NULL;
static int (*real_pthread_mutex_init)(pthread_mutex_t*, const pthread_mutexattr_t*) = NULL;
static int (*real_pthread_mutex_lock)(pthread_mutex_t*) = NULL;
static int (*real_pthread_mutex_unlock)(pthread_mutex_t*) = NULL;

static void pthread_create_init() {
    // c++ doesn't support casting void pointers to function pointers, workaround:
    // http://stackoverflow.com/questions/1096341/function-pointers-casting-in-c
    static_assert(sizeof(void *) == sizeof(real_pthread_create), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_create) = dlsym(RTLD_NEXT, "pthread_create");
    if (real_pthread_create == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_create, dlsym: %s\n", dlerror());
    }
}

static void pthread_join_init() {
    static_assert(sizeof(void *) == sizeof(real_pthread_join), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_join) = dlsym(RTLD_NEXT, "pthread_join");
    if (real_pthread_join == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_join, dlsym: %s\n", dlerror());
    }
}

static void pthread_exit_init() {
    static_assert(sizeof(void *) == sizeof(real_pthread_exit), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_exit) = dlsym(RTLD_NEXT, "pthread_exit");
    if (real_pthread_exit == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_exit, dlsym: %s\n", dlerror());
    }
}

static void pthread_mutex_init_init() {
    static_assert(sizeof(void *) == sizeof(real_pthread_mutex_init), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_mutex_init) = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    if (real_pthread_mutex_init == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_init, dlsym: %s\n", dlerror());
    }
}

static void pthread_mutex_lock_init() {
    static_assert(sizeof(void *) == sizeof(real_pthread_mutex_lock), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_mutex_lock) = dlsym(RTLD_NEXT, "pthread_mutex_lock");
    if (real_pthread_mutex_lock == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock, dlsym: %s\n", dlerror());
    }
}

static void pthread_mutex_unlock_init() {
    static_assert(sizeof(void *) == sizeof(real_pthread_mutex_unlock), "pointer cast impossible");
    *reinterpret_cast<void**>(&real_pthread_mutex_unlock) = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
    if (real_pthread_mutex_unlock == NULL) {
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_unlock, dlsym: %s\n", dlerror());
    }
}

static void init_real_pthreads() {
    // initialize real pthread function pointers
    pthread_create_init();
    pthread_join_init();
    pthread_exit_init();
    pthread_mutex_init_init();
    pthread_mutex_lock_init();
    pthread_mutex_unlock_init();
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, 
                   void *(*start_routine) (void *), void *arg) {

    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    // lock this all up in case of threads creating threads
    int res = real_pthread_mutex_lock(&thread_lock);
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
    //threadStacks.push_back("");
    sigCounts.push_back(0);

    res = real_pthread_mutex_unlock(&thread_lock);
    if (res) {
        // looking at man pages, this would mean thread_lock did not own this lock somehow
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
    int res = real_pthread_mutex_lock(&thread_lock);
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
    res = real_pthread_mutex_unlock(&thread_lock);
    if (res) {
        // likely means thread_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_join: could not unlock thread: %d\n", res);
    }
    return ret;
}

void pthread_exit(void *value_ptr) {    // TODO this function has not been (well) tested
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    int res = real_pthread_mutex_lock(&thread_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_exit: could not lock thread: %d\n", res);
        return;
    }
    unsigned int t;
    for (t = 0; t < totalThreadCount; ++t) {
        if (clientThreads[t] && pthread_equal(*clientThreads[t], pthread_self())) {
            clientThreads[t] = 0;
            break;  // found the thread
        }
    }
    if (--activeThreadCount == 0) {
        finish();    // last thread, cleanup, output traces
    }
    res = real_pthread_mutex_unlock(&thread_lock);
    if (res) {
        // likely means thread_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_exit: could not unlock thread: %d\n", res);
    }
    real_pthread_exit(value_ptr);
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    static const pthread_mutex_t temp = PTHREAD_MUTEX_INITIALIZER;
    memcpy(mutex, &temp, sizeof(pthread_mutex_t));
    contention[mutex] = 0;
    return 0;
}

static bool deadlockDetectRecur(int thread, pthread_mutex_t *mutex, int threadRequesting) {
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
    return deadlockDetectRecur(holder, threadWaiters[holder], threadRequesting);
}

// returns 1 if given thread trying to acquire given mutex creates a deadlock, else 0
static bool deadlockDetect(int thread, pthread_mutex_t *mutex) {
    return deadlockDetectRecur(thread, mutex, thread);
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!real_pthread_initialized.test_and_set()) {
        init_real_pthreads();
    }
    int res = real_pthread_mutex_lock(&mutex_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not lock thread: %d\n", res);
        return real_pthread_mutex_lock(mutex);
    }
    unsigned int t;
    for (t = 0; t < totalThreadCount; ++t) {
        if (clientThreads[t] && pthread_equal(*clientThreads[t], pthread_self())) {
            //printf("thread %d requested lock %d\n", t, mutex);
            break;
        }
    }
    // TODO make sure the thread was found, and if not either initialize it like in pthread_create or report error
    int ret = 0;
    if (!pthread_mutex_trylock(mutex)) {
        // mutex acquired
        lockHolders[mutex] = t;
    } else {
        // mutex not acquired
        threadWaiters[t] = mutex;
        if (deadlockDetect(t, mutex)) {
            // TODO slight chance that a deadlock will not actually occur,
            // possible to check here, but it is released before the actual lock attempt
            // although, the deadlock chance still exists, so not necessarily bad to print error
            printf("DEADLOCK DETECTED\n");
            // TODO maybe do a stack trace and quit?
        }
        res = real_pthread_mutex_unlock(&mutex_lock);
        if (res) {
            // likely means mutex_lock does not own the mutex somehow
            fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not unlock thread: %d\n", res);
        }
        ret = real_pthread_mutex_lock(mutex);
        res = real_pthread_mutex_lock(&mutex_lock);
        if (res) {
            // if unable to lock, just try not to disrupt host program
            fprintf(stderr, "ERROR: RAIN: pthread_mutex_lock: could not lock thread: %d\n", res);
            return ret;
        }
        lockHolders[mutex] = t;
        threadWaiters.erase(t);
    }
    res = real_pthread_mutex_unlock(&mutex_lock);
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
    int res = real_pthread_mutex_lock(&mutex_lock);
    if (res) {
        // if unable to lock, just try not to disrupt host program
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_unlock: could not lock thread: %d\n", res);
        return real_pthread_mutex_unlock(mutex);
    }
    unsigned int t;
    for (t = 0; t < totalThreadCount; ++t) {
        if (clientThreads[t] && pthread_equal(*clientThreads[t], pthread_self())) {
            //printf("thread %d releasing lock %d\n", t, mutex);
            break;
        }
    }
    int ret = real_pthread_mutex_unlock(mutex);
    lockHolders[mutex] = -1;
    //printf("thread %d released lock %d\n", t, mutex);
    real_pthread_mutex_unlock(&mutex_lock);
    if (res) {
        // likely means mutex_lock does not own the mutex somehow
        fprintf(stderr, "ERROR: RAIN: pthread_mutex_unlock: could not unlock thread: %d\n", res);
    }
    return ret;
}

