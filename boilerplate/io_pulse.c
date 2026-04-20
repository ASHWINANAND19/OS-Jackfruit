#include <stdio.h>
#include <unistd.h>

int main() {
    while (1) {
        printf("[io_pulse] working...\n");
        sleep(1);
    }
    return 0;
}