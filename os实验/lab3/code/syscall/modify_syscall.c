#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kallsyms.h> 
#include <linux/kprobes.h> 

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Experiment: modify a syscall on Linux 5.10");

/* 要篡改的系统调用号：SYS_gettimeofday */
#define SYS_HOOK_NO 96

/* CR0 中的 WP 写保护位，用来临时关闭内核只读页保护 */
#define CR0_WP_BIT 0x00010000UL

/* 记录原来的系统调用函数指针 */
static unsigned long old_syscall_func = 0;

/*
 * sys_call_table 在当前内核中的地址
 */
static unsigned long sys_call_table_addr = 0xffffffff9c0002c0;

/* 
 * 清除 CR0 中的 WP 位，使得内核代码页临时可写。
 * 返回修改前的 cr0 值，方便之后恢复。
 */
static unsigned long clear_wp(void)
{
    unsigned long cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    asm volatile("mov %0, %%cr0" :: "r"(cr0 & ~CR0_WP_BIT));
    return cr0;
}

/* 恢复 CR0 原来的值，重新开启写保护 */
static void restore_wp(unsigned long cr0)
{
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

/*
 * “伪系统调用”实现: 接受两个整数参数，返回它们的和。
 */
static asmlinkage long sys_hello(const struct pt_regs *regs)
{
    long a = regs->di;
    long b = regs->si;
    long r = a + b;
    printk(KERN_INFO "[sys_hello] syscall %d hooked: a=%ld, b=%ld, result=%ld\n",
           SYS_HOOK_NO, a, b, r);
    return r;
}

/*
 * 修改 sys_call_table，将 sys_call_table[SYS_HOOK_NO] 替换为 sys_hello。
 */
static int hook_syscall(void)
{
    unsigned long *syscall_entry;
    unsigned long old_cr0;

    if (!sys_call_table_addr) {
        pr_err("[sys_hook] sys_call_table_addr is 0 — please set it correctly.\n");
        return -EINVAL;
    }

    /* 找到 sys_call_table 中第 SYS_HOOK_NO 项 */
    syscall_entry = (unsigned long *)(sys_call_table_addr +
                                      SYS_HOOK_NO * sizeof(void *));
    /* 保存原来的函数指针 */
    old_syscall_func = *syscall_entry;

    /* 临时关闭写保护，写入新的函数地址 */
    old_cr0 = clear_wp();
    *syscall_entry = (unsigned long)sys_hello;
    restore_wp(old_cr0);

    pr_info("[sys_hook] replaced syscall %d at %p, old=0x%lx, new=%p\n",
            SYS_HOOK_NO, syscall_entry, old_syscall_func, sys_hello);
    return 0;
}

/*
 * 恢复原来的系统调用。
 * 在模块卸载时调用，保证内核不会一直处于被篡改状态。
 */
static void unhook_syscall(void)
{
    unsigned long *syscall_entry;
    unsigned long old_cr0;

    /* 如果没成功 hook 过，直接返回 */
    if (!sys_call_table_addr || !old_syscall_func)
        return;

    syscall_entry = (unsigned long *)(sys_call_table_addr +
                                      SYS_HOOK_NO * sizeof(void *));
    old_cr0 = clear_wp();
    *syscall_entry = old_syscall_func;
    restore_wp(old_cr0);

    pr_info("[sys_hook] restored syscall %d to 0x%lx\n",
            SYS_HOOK_NO, old_syscall_func);
}

/* 模块加载入口：完成系统调用表篡改 */
static int __init sys_hook_init(void)
{
    pr_info("[sys_hook] module loading, trying to hook syscall %d\n",
            SYS_HOOK_NO);
    return hook_syscall();
}

/* 模块卸载入口：恢复系统调用表 */
static void __exit sys_hook_exit(void)
{
    pr_info("[sys_hook] module unloading, restoring syscall %d\n",
            SYS_HOOK_NO);
    unhook_syscall();
}

module_init(sys_hook_init);
module_exit(sys_hook_exit);
