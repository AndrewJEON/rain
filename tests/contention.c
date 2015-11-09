/*
 * A program with lock contention
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

const int DATA_COUNT = 10;
int shared[DATA_COUNT] = {0};
pthread_mutex_t lock[DATA_COUNT];

void *work(void *arg) {
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < DATA_COUNT; j++) {
            pthread_mutex_lock(&lock[j]);
            ++shared[j];
            usleep(1000);
            pthread_mutex_unlock(&lock[j]);
        }
    }
    return arg;
}

int main() {
    int ret;
    const int THREAD_COUNT = 10;
    pthread_t thread[THREAD_COUNT];

    for (int i = 0; i < DATA_COUNT; i++) {
        ret = pthread_mutex_init(&lock[i], 0);
        if (ret) {
            fprintf(stderr, "pthred_mutex_init, error: %d\n", ret);
            return ret;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        ret = pthread_create(&thread[i], 0, work, 0);
        if (ret) {
            fprintf(stderr, "pthread_create, error: %d\n", ret);
            return ret;
        }
    }

    for (int i = 0; i < THREAD_COUNT; i++) {
        ret = pthread_join(thread[i], 0);
        if (ret) {
            fprintf(stderr, "pthread_join, error: %d\n", ret);
            return ret;
        }
    }

    for (int i = 0; i < DATA_COUNT; i++) {
        ret = pthread_mutex_destroy(&lock[i]);
        if (ret) {
            fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
            return ret;
        }
    }

    printf("data: ");
    for (int i = 0; i < DATA_COUNT; i++) {
        printf("%d, ", shared[i]);
    }
    printf("\n");

    return 0;
}
