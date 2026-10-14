#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void)
{
    struct timeval tv;
    long ret = syscall(SYS_gettimeofday, &tv, NULL); 
    if (ret == -1) {
        perror("syscall");
        return 1;
    }

    printf("tv_sec:%ld\n", (long)tv.tv_sec);
    printf("tv_usec:%ld\n", (long)tv.tv_usec);
    return 0;
}
