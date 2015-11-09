/*
 * A program to test pthreads recursively
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

void *fib(void *x) {
    int val = *((int*)x);
    if (val <= 1) {
        int *r = (int*)malloc(sizeof(int));
        *r = 1;
        return r;
    }

    int ret;
    pthread_t t1, t2;
    int *val1 = (int*)malloc(sizeof(int));
    int *val2 = (int*)malloc(sizeof(int));
    *val1 = val - 1;
    *val2 = val - 2;
    ret = pthread_create(&t1, 0, fib, val1);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return 0;
    }
    ret = pthread_create(&t2, 0, fib, val2);
    if (ret) {
        fprintf(stderr, "pthread_create, error: %d\n", ret);
        return 0;
    }

    int *r1, *r2;
    ret = pthread_join(t1, (void**)&r1);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return 0;
    }
    ret = pthread_join(t2, (void**)&r2);
    if (ret) {
        fprintf(stderr, "pthread_join, error: %d\n", ret);
        return 0;
    }

    int *r = (int*)malloc(sizeof(int));
    *r = *r1 + *r2;

    free(val1);
    free(val2);
    free(r1);
    free(r2);
    return r;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "./fib <number>\n");
        return 0;
    }

    int val = atoi(argv[1]);
    int *p = (int*)malloc(sizeof(int));
    *p = val;
    int *r = (int*)fib(p);
    printf("fib(%d) = %d\n", val, *r);
    free(p);
    free(r);
    return 0;
}
