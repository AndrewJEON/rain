/*
 * Threads which spawn other imbalanced threads
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

typedef struct Worker {
    unsigned int workTime;
} Worker;

typedef struct Group {
    int startIndex;
    int endIndex;
} Group;

// the two threads to spawn workers
pthread_t thread1, thread2;

// workers and their threads
const int WORKER_COUNT = 10;
pthread_t threads[WORKER_COUNT];
Worker workers[WORKER_COUNT];
Group groupOne, groupTwo;

void *work(void *arg) {
    Worker *w = (Worker*)arg;

    // busy wait until work time finished
    struct timeval start, cur;
    gettimeofday(&start, NULL);
    //clock_t start = clock();
    while (1) {
        gettimeofday(&cur, NULL);
        if (cur.tv_sec - start.tv_sec >= w->workTime) {
            break;
        }
    }
    // cast to inptr_t before void* to get rid of compiler warnings
    return (void*)(intptr_t)w->workTime;
}

void *makeWorkers(void *arg) {
    Group *g = (Group*)arg;
    for (int i = g->startIndex; i <= g->endIndex; i++) {
        int ret = pthread_create(&threads[i], 0, work, &workers[i]);
        if (ret) {
            fprintf(stderr, "pthread_create, error: %d\n", ret);
            return arg;
        }
    }

    for (int i = g->startIndex; i <= g->endIndex; i++) {
        int ret = pthread_join(threads[i], 0);
        if (ret) {
            fprintf(stderr, "pthread_join, error: %d\n", ret);
            return arg;
        }
    }
    return arg;
}

int main() {
    int ret;
    srand(time(NULL));

    groupOne.startIndex = 0;
    groupOne.endIndex = (WORKER_COUNT / 2.0 - 0.5);
    groupTwo.startIndex = groupOne.endIndex + 1;
    groupTwo.endIndex = WORKER_COUNT - 1;

    for (int i = 0; i < WORKER_COUNT; i++) {
        int t = rand() % 10;
        printf("Expecting %d secs of work from %d\n", t, i);
        workers[i].workTime = t;
    }

    ret = pthread_create(&thread1, 0, makeWorkers, &groupOne);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }
    ret = pthread_create(&thread2, 0, makeWorkers, &groupTwo);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }

    ret = pthread_join(thread1, 0);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }
    ret = pthread_join(thread2, NULL);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }

    return 0;
}
