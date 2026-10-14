#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

int main(void)
{
    long ret = syscall(SYS_gettimeofday, 10, 20); 
    printf("%ld\n", ret);
    return 0;
}
