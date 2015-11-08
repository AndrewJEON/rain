/*
 * A program that will (probably) not deadlock
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

pthread_t thread1, thread2;
static pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;

void *workOne(void *arg) {
    pthread_mutex_lock(&lock1);
    usleep(1000);
    pthread_mutex_unlock(&lock1);
    pthread_mutex_lock(&lock2);
    usleep(1000);
    pthread_mutex_unlock(&lock2);
    return arg;
}

void *workTwo(void *arg) {
    usleep(1000);
    pthread_mutex_lock(&lock1);
    usleep(1000);
    pthread_mutex_unlock(&lock1);
    pthread_mutex_lock(&lock2);
    usleep(1000);
    pthread_mutex_unlock(&lock2);
    return arg;
}

int main() {
    int ret;

    ret = pthread_create(&thread1, 0, workOne, 0);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }
    ret = pthread_create(&thread2, 0, workTwo, 0);
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
    return 0;
}
