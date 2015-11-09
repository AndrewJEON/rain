/*
 * A program with various lock errors
 */

#include <pthread.h>
#include <stdio.h>

pthread_mutex_t lock;

void *work(void *arg) {
    pthread_mutex_unlock(&lock);    // unlock a not locked mutex
    pthread_mutex_lock(&lock);      // never released
    return arg;
}

int main() {
    int ret;
    pthread_t thread1, thread2;

    ret = pthread_mutex_init(&lock, 0);
    if (ret) {
        fprintf(stderr, "pthread_mutex_init, error: %d\n", ret);
        return ret;
    }

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

    ret = pthread_mutex_destroy(&lock);   // still locked
    if (ret) {
        fprintf(stderr, "pthread_mutex_destroy, error: %d\n", ret);
        return ret;
    }
    return 0;
}
