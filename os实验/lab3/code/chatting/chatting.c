#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>

#define CHAT_MAJOR 290
#define BUF_SIZE   4096 // 4KB buffer

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Name");
MODULE_DESCRIPTION("Multi-user Chat Room Device");

struct chat_dev_t {
    char buf[BUF_SIZE];
    unsigned long head;       // Global write pointer
    struct mutex lock;        // Protects buf and head
    wait_queue_head_t wq;     // Wait queue for readers
};

static struct chat_dev_t chat_dev;

// Per-file private data
struct chat_private {
    unsigned long tail;       // Local read pointer
};

static int chat_open(struct inode *inode, struct file *filp)
{
    struct chat_private *priv;

    priv = kmalloc(sizeof(struct chat_private), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    mutex_lock(&chat_dev.lock);
    // New user starts reading from the oldest available data (history)
    if (chat_dev.head > BUF_SIZE)
        priv->tail = chat_dev.head - BUF_SIZE;
    else
        priv->tail = 0;
    mutex_unlock(&chat_dev.lock);

    filp->private_data = priv;
    return 0;
}

static int chat_release(struct inode *inode, struct file *filp)
{
    kfree(filp->private_data);
    return 0;
}

static ssize_t chat_read(struct file *filp, char __user *buf,
                         size_t len, loff_t *off)
{
    struct chat_private *priv = filp->private_data;
    int ret = 0;
    unsigned long count;
    int i;

    // Wait until there is new data (head > tail)
    if (wait_event_interruptible(chat_dev.wq, chat_dev.head != priv->tail))
        return -ERESTARTSYS;

    mutex_lock(&chat_dev.lock);

    // Check for buffer overflow (reader too slow)
    if (chat_dev.head - priv->tail > BUF_SIZE) {
        priv->tail = chat_dev.head - BUF_SIZE;
    }

    count = chat_dev.head - priv->tail;
    if (count > len)
        count = len;

    // Copy data to user
    for (i = 0; i < count; i++) {
        char c = chat_dev.buf[(priv->tail + i) & (BUF_SIZE - 1)]; // Circular buffer
        if (put_user(c, buf + i)) { 
            // Failed when return a non-zero value
            ret = -EFAULT;
            goto out;
        }
    }

    priv->tail += count;
    ret = count;

out:
    mutex_unlock(&chat_dev.lock);
    return ret;
}

static ssize_t chat_write(struct file *filp, const char __user *buf,
                          size_t len, loff_t *off)
{
    int ret = 0;
    int i;

    if (len > BUF_SIZE)
        return -EINVAL; // Message too large

    mutex_lock(&chat_dev.lock);

    for (i = 0; i < len; i++) {
        char c;
        if (get_user(c, buf + i)) {
            ret = -EFAULT;
            goto out;
        }
        chat_dev.buf[(chat_dev.head + i) & (BUF_SIZE - 1)] = c;
    }

    chat_dev.head += len;
    ret = len;

    // Wake up all readers
    wake_up_interruptible(&chat_dev.wq);

out:
    mutex_unlock(&chat_dev.lock);
    return ret;
}

static __poll_t chat_poll(struct file *filp, poll_table *wait)
{
    struct chat_private *priv = filp->private_data;
    __poll_t mask = 0;

    poll_wait(filp, &chat_dev.wq, wait);

    mutex_lock(&chat_dev.lock);
    if (chat_dev.head != priv->tail)
        mask |= EPOLLIN | EPOLLRDNORM;
    mutex_unlock(&chat_dev.lock);

    return mask;
}

static struct file_operations chat_fops = {
    .owner   = THIS_MODULE,
    .read    = chat_read,
    .write   = chat_write,
    .poll    = chat_poll,
    .open    = chat_open,
    .release = chat_release,
};

static int __init chat_init(void)
{
    int ret;

    // Initialize device structure
    mutex_init(&chat_dev.lock);
    init_waitqueue_head(&chat_dev.wq);
    chat_dev.head = 0;

    ret = register_chrdev(CHAT_MAJOR, "chatdev", &chat_fops);
    if (ret < 0) {
        printk(KERN_ERR "chatdev: register_chrdev failed\n");
        return ret;
    }

    printk(KERN_INFO "chatdev: module loaded\n");
    return 0;
}

static void __exit chat_exit(void)
{
    unregister_chrdev(CHAT_MAJOR, "chatdev");
    printk(KERN_INFO "chatdev: module unloaded\n");
}

module_init(chat_init);
module_exit(chat_exit);
