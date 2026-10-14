#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
int pid1, pid2; // 定义两个进程变量
int main() {
    int fd[2];
    char InPipe[4001]; // 定义读缓冲区
    char OutPipe[4001]; // 定义写缓冲区
    char c1 = '1', c2 = '2';
    if (pipe(fd) == -1) { // 创建管道
        perror("pipe");
        exit(1);
    }
    while ((pid1 = fork()) == -1); // 如果进程1创建不成功,则空循环
    // 如果子进程1创建成功,pid1为进程号
    if (pid1 == 0) {            
        lockf(fd[1], 1, 0);          // TODO:锁定管道
        // 将"Child process 1 is sending message!"写入OutPipe
        // snprintf(OutPipe, sizeof(OutPipe), "Child process 1 is sending message!\n");
        // write(fd[1], &OutPipe, strlen(OutPipe));

        // TODO:分2000次每次向管道写入字符’1’
        for (int i = 0; i < 2000; i++) {
            write(fd[1], &c1, 1);
        }

        sleep(5); // 等待读进程读出数据
        lockf(fd[1], 0, 0);          // TODO:解除管道的锁定
        exit(0);  // 结束进程1
    } else {
        while ((pid2 = fork()) == -1); // 若进程2创建不成功,则空循环
        if (pid2 == 0) {
            lockf(fd[1], 1, 0);
            // 将"Child process 2 is sending message!"写入OutPipe
            // snprintf(OutPipe, sizeof(OutPipe), "Child process 2 is sending message!\n");
            // write(fd[1], &OutPipe, strlen(OutPipe));

            // TODO:分2000次每次向管道写入字符’2’
            for (int i = 0; i < 2000; i++) {
                write(fd[1], &c2, 1);
            }
            sleep(5);
            lockf(fd[1], 0, 0);
            exit(0);
        } else {
            wait(0);                // TODO:等待子进程1结束
            wait(0);                // 等待子进程2结束
            read(fd[0], InPipe, 4000);     // TODO:从管道中读出4000个字符
            InPipe[4000] = '\0';           //  TODO:加字符串结束符
            printf("%s\n", InPipe); // 显示读出的数据
            exit(0);                // 父进程结束
        }
    }
}
