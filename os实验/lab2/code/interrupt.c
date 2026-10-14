#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
int flag = 0;
void inter_handler(int sig) {
    printf("\n %d stop test.\n", sig);
    flag = sig;
}
void waiting() {
    // alarm(5);
    // while (!flag) {
    //     pause();
    // }
    sleep(5);
    if (flag == 0) kill(getpid(), SIGINT);
}
int main() {
    // TODO: 五秒之后接收到两个信号
    pid_t pid1 = -1, pid2 = -1;
    while (pid1 == -1)
        pid1 = fork();
    if (pid1 > 0) {
        while (pid2 == -1)
            pid2 = fork();
        if (pid2 > 0) {
            // 父进程
            // SIGINT = 2, SIGQUIT = 3
            signal(SIGINT, inter_handler);
            signal(SIGQUIT, inter_handler);
            signal(SIGALRM, inter_handler);
            waiting();
            kill(pid1, SIGSTKFLT);
            kill(pid2, SIGCHLD);
            waitpid(pid1, NULL, 0);
            waitpid(pid2, NULL, 0);
            printf("\nParent process is killed!!\n");
        }
        else {
            // 子进程 2
            signal(SIGINT, SIG_IGN);
            signal(SIGQUIT, SIG_IGN);
            signal(SIGCHLD, inter_handler);
            pause();
            printf("\nChild process2 is killed by parent!!\n");
            return 0;
        }
    }
    else {
        // 子进程1
        signal(SIGINT, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGSTKFLT, inter_handler);
        pause();
        printf("\nChild process1 is killed by parent!!\n");
        return 0;
    }
    return 0;
}