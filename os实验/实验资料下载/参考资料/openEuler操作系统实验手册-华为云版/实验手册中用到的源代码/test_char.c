//
// test_char.c
//
// The default type of char is 'unsigned char' in Kunpeng Platform
// gcc test_char.c

#include <stdio.h>

#define JUDGE_NEGATIVE_OR_NOT(x) \
        ((x) < 0 ? printf("negative") : \
        ((x) == 0) ? printf("zero") : \
        printf("positive"))

int main()
{
    unsigned char ch1 = -1;
             char ch2 = -1;
      signed char ch3 = -1;

    //    original code of  1: 0000 0001b
    // ones' complement of  1: 1111 1110b
    // two's complement of -1: 1111 1111b
    printf("unsigned char ch1 = 0x%x, %d, ", ch1, ch1), JUDGE_NEGATIVE_OR_NOT(ch1), printf("\n");
    printf("         char ch2 = 0x%x, %d, ", ch2, ch2), JUDGE_NEGATIVE_OR_NOT(ch2), printf("\n");
    printf("  signed char ch3 = 0x%x, %d, ", ch3, ch3), JUDGE_NEGATIVE_OR_NOT(ch3), printf("\n");

    return 0;
}