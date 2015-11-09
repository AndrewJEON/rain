#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define NOTHREADS 2

/*
gcc -ggdb -lpthread parallel-mergesort.c 
NOTE: 
The mergesort boils downs to this.. 
Given two sorted array's how do we merge this?
We need a new array to hold the result of merging
otherwise it is not possible to do it using array, 
so we may need a linked list
*/

/*
 * Original source:
 * https://github.com/sangeeths/stackoverflow/blob/master/two-threads-parallel-merge-sort.c
 */

//int a[] = {10, 8, 5, 2, 3, 6, 7, 1, 4, 9};
int *a;

typedef struct node {
    int i;
    int j;
} NODE;

void merge(int i, int j)
{
    int mid = (i+j)/2;
    int ai = i;
    int bi = mid+1;

    int newa[j-i+1], newai = 0;

    while(ai <= mid && bi <= j) {
            if (a[ai] > a[bi])
                    newa[newai++] = a[bi++];
            else                    
                    newa[newai++] = a[ai++];
    }

    while(ai <= mid) {
            newa[newai++] = a[ai++];
    }

    while(bi <= j) {
            newa[newai++] = a[bi++];
    }

    for (ai = 0; ai < (j-i+1) ; ai++)
            a[i+ai] = newa[ai];
}

void * mergesort(void *a)
{
    NODE *p = (NODE *)a;
    NODE n1, n2;
    int mid = (p->i+p->j)/2;
    pthread_t tid1, tid2;
    int ret;

    n1.i = p->i;
    n1.j = mid;

    n2.i = mid+1;
    n2.j = p->j;

    if (p->i >= p->j) return 0;

    ret = pthread_create(&tid1, NULL, mergesort, &n1);
    if (ret) {
            printf("%d %s - unable to create thread - ret - %d\n", __LINE__, __FUNCTION__, ret);    
            exit(1);
    }


    ret = pthread_create(&tid2, NULL, mergesort, &n2);
    if (ret) {
            printf("%d %s - unable to create thread - ret - %d\n", __LINE__, __FUNCTION__, ret);    
            exit(1);
    }

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    merge(p->i, p->j);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        fprintf(stderr, "mergesort <space seperated integers>\n");
        return 0;
    }

    int count = argc - 1;
    a = (int*)malloc(sizeof(int) * count);
    for (int i = 0; i < count; i++) {
        a[i] = atoi(argv[i + 1]);
    }

    int i;
    NODE m;
    m.i = 0;
    m.j = count - 1;
    pthread_t tid;

    int ret; 

    ret=pthread_create(&tid, NULL, mergesort, &m);
    if (ret) {
            printf("%d %s - unable to create thread - ret - %d\n", __LINE__, __FUNCTION__, ret);    
            exit(1);
    }

    pthread_join(tid, NULL);

    for (i = 0; i < count; i++)
                    printf ("%d ", a[i]);

    printf ("\n");
    free(a);

    // pthread_exit(NULL);
    return 0;
}

