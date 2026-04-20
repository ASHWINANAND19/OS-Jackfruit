#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage: ./memory_hog <MB>\n");
        return 1;
    }

    int mb = atoi(argv[1]);
    char *mem = malloc(mb * 1024 * 1024);

    if (!mem) {
        printf("Memory allocation failed\n");
        return 1;
    }

    printf("[memory_hog] Allocated %d MB\n", mb);

    while (1) {
        for (int i = 0; i < mb * 1024 * 1024; i++) {
            mem[i] = 1;
        }
        sleep(1);
    }

    return 0;
}