// abc.c
//
// "gcc -S abc.c" to generates assembly code
// "gcc abc.c" to generates binary code
//

#include <stdio.h>

int main()
{
    int a = 1;
    int b = 2;
    int c = a + b;
    printf("c = %d\n", c);

    return 0;
} 