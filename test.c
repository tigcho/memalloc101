#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "memalloc102.h"

void *thread_func(void *arg) {
    char *ptr = (char *)malloc(100);
    if (ptr) {
        strcpy(ptr, "Hello, World!");
        printf("%s\n", ptr);
        free(ptr);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    char *ptr1 = (char *)malloc(50);
    if (ptr1) {
        strcpy(ptr1, "Test String 1");
        printf("%s\n", ptr1);
        free(ptr1);
    }

    pthread_create(&t1, NULL, thread_func, NULL);
    pthread_create(&t2, NULL, thread_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}