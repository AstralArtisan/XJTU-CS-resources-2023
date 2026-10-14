#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

// 打印当前进程和线程信息
void print_ids(const char *msg) {
    pid_t pid = getpid();
    pthread_t tid = syscall(SYS_gettid);
    printf("[%s] PID = %d, TID = %lu\n", msg, pid, tid);
}

// 线程1：在子线程中调用 system()
void* thread_system(void* arg) {
    print_ids("thread_system");
    // 调用 system_call 程序
    system("./system_call");
    return NULL;
}

// 线程2：在子线程中调用 exec()
void* thread_exec(void* arg) {
    print_ids("thread_exec");
    char *args[] = {"./system_call", NULL};
    execvp(args[0], args);   // 替换当前线程所在的整个进程映像
    perror("exec failed");
    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, thread_system, NULL);
    sleep(1); // 确保 thread_system 先运行
    pthread_create(&t2, NULL, thread_exec, NULL);
    sleep(1);

    pthread_join(t1, NULL);
    //pthread_join(t2, NULL);

    return 0;
}