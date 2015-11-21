/*
 * A program with a load imbalance
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

pthread_t thread1, thread2;
typedef struct Worker {
    unsigned int workTime;
} Worker;

void *workONE(void *arg) {
    Worker *w = (Worker*)arg;

    // busy wait until work time finished
    struct timeval start, cur;
    gettimeofday(&start, NULL);
    while (1) {
        gettimeofday(&cur, NULL);
        if (cur.tv_sec - start.tv_sec >= w->workTime) {
            break;
        }
    }
    // cast to intptr_t before void* to get rid of compiler warnings
    return (void*)(intptr_t)w->workTime;
}

void *workTWO(void *arg) {
    Worker *w = (Worker*)arg;

    // busy wait until work time finished
    struct timeval start, cur;
    gettimeofday(&start, NULL);
    while (1) {
        gettimeofday(&cur, NULL);
        if (cur.tv_sec - start.tv_sec >= w->workTime) {
            break;
        }
    }
    // cast to intptr_t before void* to get rid of compiler warnings
    return (void*)(intptr_t)w->workTime;
}

void printUsage() {
    fprintf(stderr, "Usage: ./two_threads_unbalanced <thread1_work_secs> <thread2_work_secs>\n");
}

int main(int argc, char *argv[]) {
    int ret;

    Worker workerOne, workerTwo;
    if (argc != 3) {
        printUsage();
        return 1;
    }

    char *end;
    errno = 0;
    workerOne.workTime = strtol(argv[1], &end, 10);
    if (errno || end == argv[1]) {
        printUsage();
        return 1;
    }
    errno = 0;
    workerTwo.workTime = strtol(argv[2], &end, 10);
    if (errno || end == argv[2]) {
        printUsage();
        return 1;
    }

    ret = pthread_create(&thread1, 0, workONE, &workerOne);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }
    ret = pthread_create(&thread2, 0, workTWO, &workerTwo);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }

    unsigned int work1, work2;
    ret = pthread_join(thread1, (void**)&work1);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }
    ret = pthread_join(thread2, (void**)&work2);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }

    printf("thread 1 worked for %u secs, thread 2 worked for %u secs\n", work1, work2);
    return 0;
}
