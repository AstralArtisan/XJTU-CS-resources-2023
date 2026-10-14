#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define COUNT 100000

int shared_var = 0;
sem_t signal;

void* add_func(void* arg) {
    for (int i = 0; i < COUNT; i++) {
        sem_wait(&signal);   // P 操作：申请资源（进入临界区）
        shared_var += 100;
        sem_post(&signal);   // V 操作：释放资源（离开临界区）
    }
    return NULL;
}

void* sub_func(void* arg) {
    for (int i = 0; i < COUNT; i++) {
        sem_wait(&signal);   // P 操作
        shared_var -= 100;
        sem_post(&signal);   // V 操作
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    sem_init(&signal, 0, 1);

    pthread_create(&t1, NULL, add_func, NULL);
    pthread_create(&t2, NULL, sub_func, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Final value of shared_var = %d\n", shared_var);

    sem_destroy(&signal);
    return 0;
}