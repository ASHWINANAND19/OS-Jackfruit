#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "monitor_ioctl.h"

#define DEVICE_NAME "container_monitor"

static int major;

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct container_info info;

    if (copy_from_user(&info, (struct container_info *)arg, sizeof(info)))
        return -EFAULT;

    switch (cmd) {
        case IOCTL_ADD_CONTAINER:
            printk(KERN_INFO "Monitor: Add PID %d\n", info.pid);
            break;

        case IOCTL_REMOVE_CONTAINER:
            printk(KERN_INFO "Monitor: Remove PID %d\n", info.pid);
            break;

        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = device_ioctl,
};

static int __init monitor_init(void)
{
    major = register_chrdev(0, DEVICE_NAME, &fops);
    printk(KERN_INFO "Monitor loaded\n");
    return 0;
}

static void __exit monitor_exit(void)
{
    unregister_chrdev(major, DEVICE_NAME);
    printk(KERN_INFO "Monitor unloaded\n");
}

module_init(monitor_init);
module_exit(monitor_exit);

MODULE_LICENSE("GPL");