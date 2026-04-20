#ifndef MONITOR_IOCTL_H
#define MONITOR_IOCTL_H

#define DEVICE_NAME "container_monitor"

#define IOCTL_ADD_CONTAINER _IOW('a', 'a', int)
#define IOCTL_REMOVE_CONTAINER _IOW('a', 'b', int)

struct container_info {
    int pid;
    unsigned long memory;
};

#endif