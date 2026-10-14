#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define MAX_PAGES 100    // 页面序列最大长度
#define MAX_FRAMES 10    // 物理块最大数量
int page_seq[MAX_PAGES]; // 页面访问序列
int frame[MAX_FRAMES];   // 当前内存块
int page_num;            // 页面序列长度
int frame_num;           // 内存块数量

// 打印当前内存状态
void print_frames() {
    for (int i = 0; i < frame_num; i++) {
        if (frame[i] == -1) printf(" - ");
        else printf(" %d ", frame[i]);
    }
    printf("\n");
}

// FIFO 页面置换算法
void fifo() {
    int page_faults = 0;
    int next_to_replace = 0; // FIFO替换指针

    for (int i = 0; i < frame_num; i++)
        frame[i] = -1;

    printf("\n*** FIFO 页面置换过程 ***\n");
    for (int i = 0; i < page_num; i++) {
        int page = page_seq[i];
        int found = 0;
        for (int j = 0; j < frame_num; j++) {
            if (frame[j] == page) {
                found = 1;
                break;
            }
        }

        if (!found) {
            frame[next_to_replace] = page;
            next_to_replace = (next_to_replace + 1) % frame_num;
            page_faults++;
            printf("缺页 -> 调入页面 %d :", page);
        } else {
            printf("命中 -> 页面 %d :", page);
        }
        print_frames();
    }

    printf("\nFIFO缺页次数: %d, 缺页率: %.2f%%\n", 
           page_faults, 100.0 * page_faults / page_num);
}

// LRU 页面置换算法
void lru() {
    int page_faults = 0;
    int last_used[MAX_FRAMES]; // 每个块最近使用的时间

    for (int i = 0; i < frame_num; i++) {
        frame[i] = -1;
        last_used[i] = -1;
    }

    printf("\n*** LRU 页面置换过程 ***\n");
    for (int t = 0; t < page_num; t++) {
        int page = page_seq[t];
        int found = 0;
        for (int j = 0; j < frame_num; j++) {
            if (frame[j] == page) {
                found = 1;
                last_used[j] = t; // 更新最近使用时间
                break;
            }
        }

        if (!found) {
            int replace_index = 0, oldest = last_used[0];
            for (int j = 1; j < frame_num; j++) {
                if (frame[j] == -1) { replace_index = j; break; }
                if (last_used[j] < oldest) {
                    oldest = last_used[j];
                    replace_index = j;
                }
            }
            frame[replace_index] = page;
            last_used[replace_index] = t;
            page_faults++;
            printf("缺页 -> 调入页面 %d :", page);
        } else {
            printf("命中 -> 页面 %d :", page);
        }
        print_frames();
    }

    printf("\nLRU缺页次数: %d, 缺页率: %.2f%%\n", 
           page_faults, 100.0 * page_faults / page_num);
}

int main() {
    int choice, mode;

    printf("请输入页面引用序列长度: ");
    scanf("%d", &page_num);
    printf("请输入物理块数量: ");
    scanf("%d", &frame_num);

    printf("选择页面序列生成方式: 1.手动输入  2.随机生成\n");
    scanf("%d", &mode);

    if (mode == 1) {
        printf("请输入页面引用序列（以空格分隔）:\n");
        for (int i = 0; i < page_num; i++)
            scanf("%d", &page_seq[i]);
    } else {
        srand(time(NULL));
        for (int i = 0; i < page_num; i++)
            page_seq[i] = rand() % 10; // 0~9的随机页号
        printf("随机生成的页面序列为:\n");
        for (int i = 0; i < page_num; i++)
            printf("%d ", page_seq[i]);
        printf("\n");
    }

    printf("\n选择算法: 1.FIFO  2.LRU\n");
    scanf("%d", &choice);

    if (choice == 1) fifo();
    else if (choice == 2) lru();
    else printf("无效选择。\n");

    return 0;
}