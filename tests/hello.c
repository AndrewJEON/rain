/*
 * A program where threads print a c-string argument
 */

#include <pthread.h>
#include <stdio.h>

void *print_arg(void *arg) {
    printf("%s", (char*)arg);
    return 0;
}

int main() {
    int ret;
    pthread_t thread1, thread2;

    char msg1[] = "Hello from thread 1\n";
    char msg2[] = "Hello from thread 2\n";

    ret = pthread_create(&thread1, 0, print_arg, (void*)msg1);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return ret;
    }
    ret = pthread_create(&thread2, 0, print_arg, (void*)msg2);
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
