/*
 * A program with lock contention
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t lock;

void *hold(void *arg) {
    pthread_mutex_lock(&lock);
    usleep(10000);
    pthread_mutex_unlock(&lock);
    return arg;
}

void *wait(void *arg) {
    pthread_mutex_lock(&lock);
    pthread_mutex_unlock(&lock);
    return arg;
}

int main() {
    int ret;
    const int THREAD_COUNT = 50;
    pthread_t thread[THREAD_COUNT];

    ret = pthread_mutex_init(&lock, 0);
    if (ret) {
        fprintf(stderr, "pthred_mutex_init, error: %d\n", ret);
        return ret;
    }

    ret = pthread_create(&thread[0], 0, hold, 0);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }

    for (int i = 1; i < THREAD_COUNT; i++) {
        ret = pthread_create(&thread[i], 0, wait, 0);
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

    ret = pthread_mutex_destroy(&lock);
    if (ret) {
        fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
        return ret;
    }

    return 0;
}
