#include <stdio.h>
#include <pthread.h>

#define COUNT 100000

int shared_var = 0; // 共享变量

void* add_func(void* arg) {
    for (int i = 0; i < COUNT; i++) {
        shared_var += 100;
    }
    return NULL;
}

void* sub_func(void* arg) {
    for (int i = 0; i < COUNT; i++) {
        shared_var -= 100;
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, add_func, NULL);
    pthread_create(&t2, NULL, sub_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final value of shared_var = %d\n", shared_var);

    return 0;
}