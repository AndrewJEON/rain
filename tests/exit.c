/*
 * A program that tests pthread_exit
 */

#include <pthread.h>
#include <stdio.h>

void *work(void *arg) {
    (void)arg;
    pthread_exit(0);
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
    return 0;
}
