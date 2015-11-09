/*
 * A program with a race condition
 */

#include <pthread.h>
#include <stdio.h>

int counter = 0;    // shared data

void *work(void *arg) {
    printf("thread %d entering\n", counter);
    for (int i = 0; i < 10000; i++) {
        ++counter;
    }
    printf("thread %d leaving\n", counter);
    return arg;
}

int main() {
    int ret;
    pthread_t thread1, thread2;

    ret = pthread_create(&thread1, 0, work, 0);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }
    ret = pthread_create(&thread2, 0, work, 0);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }

    ret = pthread_join(thread1, 0);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }
    ret = pthread_join(thread2, 0);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }

    printf("counter: %d\n", counter);
    return 0;
}
