#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main()
{
    pid_t pid, pid1;

    /* fork a child process */
    pid = fork();

    if (pid < 0) { /* error occurred */
        fprintf(stderr, "Fork Failed\n");
        return 1;
    }
    else if (pid == 0) { /* child process */
        pid1 = getpid();
        printf("child process1 PID: %d\n", pid1);

        // 使用 system()
        // system("./system_call");

        // 使用 execvp()
        char *args[] = {"./system_call", NULL};
        execvp(args[0], args);
        perror("exec failed");
        exit(1);
    }
    else { /* parent process */
        pid1 = getpid();
        printf("parent process PID: %d\n", pid1);
        wait(NULL);
    }

    return 0;
}