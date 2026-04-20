#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int duration = 20;

    if (argc > 1)
        duration = atoi(argv[1]);

    printf("[cpu_hog] Running for %d seconds (PID=%d)\n", duration, getpid());

    for (int i = 0; i < duration; i++) {
        for (volatile long j = 0; j < 100000000; j++);
    }

    printf("[cpu_hog] Done\n");
    return 0;
}