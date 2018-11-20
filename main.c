#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fcntl.h>
#include <signal.h>


int main() {
    int fd1;
    char *fifoFile = "/tmp/projekt";
    mkfifo(fifoFile, 0666);

    int fd2[2];
    if(pipe(fd2)) {
        fprintf(stderr, "Error creating pipe");
        return 1;
    }


    if (!fork()) {
        char x;
        fd1 = open(fifoFile, O_WRONLY);
        while (read(STDIN_FILENO, &x, 1)) {
            write(fd1, &x, sizeof(x));
        }
        close(fd1);
        return 0;
    }

    if (!fork()) {
        close(fd2[0]);

        char x;
        fd1 = open(fifoFile, O_RDONLY);

        unsigned long long count = 0;

        while (read(fd1, &x, 1)) {
            write(fd2[1], &x, sizeof(x));
            count++;
        }
        close(fd2[1]);
        fprintf(stderr, "Proces 2 odczytał: %lld bytes\n", count);
        unlink(fifoFile);
        return 0;
    }

    if(!fork()) {
        close(fd2[1]);
        char x;
        fprintf(stdout, "Proces 3 odebrał:\n");
        while (read(fd2[0], &x, 1)) {
            write(STDOUT_FILENO, &x, sizeof(x));
        }
        close(fd2[0]);
        return 0;
    }


    return 0;
}