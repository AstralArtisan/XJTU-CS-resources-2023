#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
    pid_t pid, pid1;
    int common = 162;
    pid = fork();
    if(pid < 0){
        fprintf(stderr, "Fork Failed");
        return 1;
    }
    else if(pid == 0){
        pid1 = getpid();
        printf("child: pid = %d\n",pid);
        printf("child: pid1 = %d\n",pid1);
        printf("child common = %d, addr = %lx\n", ++common, (unsigned long)&common);
    }
    else {
        pid1 = getpid();
        printf("parent: pid = %d\n",pid);
        printf("parent: pid1 = %d\n",pid1);     
        printf("parent common = %d, addr = %lx\n", --common, (unsigned long)&common); 
        wait(NULL);
    }
    common += 10;
    printf("common = %d, addr = %lx\n", common, (unsigned long)&common);
    return 0;
}