/*
 * A program that will (probably) deadlock
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>
#include <unistd.h>

pthread_t thread1, thread2, thread3;
pthread_mutex_t lock1, lock2, lock3;

void *workOne(void *arg) {
    pthread_mutex_lock(&lock1);
    usleep(1000);
    pthread_mutex_lock(&lock2);
    usleep(1000);
    pthread_mutex_unlock(&lock2);
    pthread_mutex_unlock(&lock1);
    return arg;
}

void *workTwo(void *arg) {
    pthread_mutex_lock(&lock2);
    usleep(1000);
    pthread_mutex_lock(&lock3);
    usleep(1000);
    pthread_mutex_unlock(&lock3);
    pthread_mutex_unlock(&lock2);
    return arg;
}

void *workThree(void *arg) {
    pthread_mutex_lock(&lock3);
    usleep(1000);
    pthread_mutex_lock(&lock1);
    usleep(1000);
    pthread_mutex_unlock(&lock1);
    pthread_mutex_unlock(&lock3);
    return arg;
}

int main() {
    int ret;

    ret = pthread_mutex_init(&lock1, 0);
    if (ret) {
        fprintf(stderr, "pthread_mutex_init, error: %d\n", ret);
        return ret;
    }
    ret = pthread_mutex_init(&lock2, 0);
    if (ret) {
        fprintf(stderr, "pthread_mutex_init, error: %d\n", ret);
        return ret;
    }
    ret = pthread_mutex_init(&lock3, 0);
    if (ret) {
        fprintf(stderr, "pthread_mutex_init, error: %d\n", ret);
        return ret;
    }

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
    ret = pthread_create(&thread3, 0, workThree, 0);
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
    ret = pthread_join(thread3, 0);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return ret;
    }

    ret = pthread_mutex_destroy(&lock3);
    if (ret) {
        fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
        return ret;
    }
    ret = pthread_mutex_destroy(&lock2);
    if (ret) {
        fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
        return ret;
    }
    ret = pthread_mutex_destroy(&lock1);
    if (ret) {
        fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
        return ret;
    }
    return 0;
}
