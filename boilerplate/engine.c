#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define FILE_DB "containers.db"

typedef struct {
    char name[50];
    int pid;
    int running;
    int soft, hard;
} Container;

// save container
void save(Container c) {
    FILE *f = fopen(FILE_DB, "a");
    if (!f) return;
    fwrite(&c, sizeof(Container), 1, f);
    fclose(f);
}

// load all containers
int load(Container arr[]) {
    FILE *f = fopen(FILE_DB, "r");
    if (!f) return 0;

    int i = 0;
    while (fread(&arr[i], sizeof(Container), 1, f))
        i++;

    fclose(f);
    return i;
}

// start container
void start(char *name, char *rootfs, char *cmd, int soft, int hard) {

    pid_t pid = fork();

    if (pid == 0) {
        chdir(rootfs);  // simulate rootfs
        execl("/bin/sh", "sh", "-c", cmd, NULL);
        perror("exec failed");
        exit(1);
    }

    Container c;
    strcpy(c.name, name);
    c.pid = pid;
    c.running = 1;
    c.soft = soft;
    c.hard = hard;

    save(c);

    printf("Started %s (PID=%d)\n", name, pid);
}

// list containers
void ps() {
    Container arr[100];
    int n = load(arr);

    printf("\nNAME\tPID\tSTATUS\tSOFT\tHARD\n");

    for (int i = 0; i < n; i++) {
        printf("%s\t%d\t%s\t%d\t%d\n",
            arr[i].name,
            arr[i].pid,
            arr[i].running ? "RUNNING" : "STOPPED",
            arr[i].soft,
            arr[i].hard);
    }
}

// logs
void logs(char *name) {
    Container arr[100];
    int n = load(arr);

    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i].name, name) == 0) {
            printf("\nLogs for %s:\n", name);
            printf("PID: %d\n", arr[i].pid);
            printf("STATUS: %s\n", arr[i].running ? "RUNNING" : "STOPPED");
            printf("SOFT LIMIT: %d\n", arr[i].soft);
            printf("HARD LIMIT: %d\n", arr[i].hard);
            return;
        }
    }

    printf("Container not found\n");
}

// stop container
void stop(char *name) {
    Container arr[100];
    int n = load(arr);

    FILE *f = fopen(FILE_DB, "w");

    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i].name, name) == 0) {
            kill(arr[i].pid, SIGKILL);
            arr[i].running = 0;
        }
        fwrite(&arr[i], sizeof(Container), 1, f);
    }

    fclose(f);

    printf("Stopped %s\n", name);
}

// supervisor loop
void supervisor() {
    printf("[engine] supervisor running...\n");
    while (1) sleep(5);
}

// main
int main(int argc, char *argv[]) {

    if (argc < 2) {
        printf("Usage:\n");
        printf("./engine supervisor <rootfs>\n");
        printf("./engine start <name> <rootfs> <cmd> [soft hard]\n");
        printf("./engine ps\n");
        printf("./engine logs <name>\n");
        printf("./engine stop <name>\n");
        return 1;
    }

    if (strcmp(argv[1], "supervisor") == 0) {
        supervisor();
    }

    else if (strcmp(argv[1], "start") == 0 && argc >= 5) {
        int soft = 0, hard = 0;

        if (argc >= 7) {
            soft = atoi(argv[5]);
            hard = atoi(argv[6]);
        }

        start(argv[2], argv[3], argv[4], soft, hard);
    }

    else if (strcmp(argv[1], "ps") == 0) {
        ps();
    }

    else if (strcmp(argv[1], "logs") == 0 && argc >= 3) {
        logs(argv[2]);
    }

    else if (strcmp(argv[1], "stop") == 0 && argc >= 3) {
        stop(argv[2]);
    }

    else {
        printf("Invalid command\n");
    }

    return 0;
}