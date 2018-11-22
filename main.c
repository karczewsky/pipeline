#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>

volatile int *pids;

void propagate_signal(int sig) {
    for(int i=0; i<3; i++) {
        if(pids[i] != getpid()) {
            union sigval s;
            s.sival_int = sig;
            printf("I try to send to %d signal: %d\n", pids[i], sig);
            sigqueue(pids[i], SIGUSR2, s);
        }
    }
    fflush(stdout);
}

void sighandler(int sig, siginfo_t *info, void *context) {
    if(sig == SIGUSR2) {
        sig = info->si_value.sival_int;
    } else {
        propagate_signal(sig);
    }

    switch (sig) {
        case SIGUSR1:
            printf("%d Ending\n", getpid());
            exit(0);
        case SIGINT:
            printf("%d Pausing\n", getpid());
            fflush(stdout);

            sigset_t set;
            sigfillset(&set);
            sigdelset(&set, SIGCONT);
            sigdelset(&set, SIGUSR2);
            sigsuspend(&set);

            break;
        case SIGCONT:
            printf("%d Resuming\n", getpid());
            fflush(stdout);
            break;
        default:
            break;
    }
}

int main() {
    struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = sighandler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGCONT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pids = mmap(NULL, 3*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if(!pids) {
        perror("Blad tworzac mmap");
        return 1;
    }

    int fd1;
    char *fifoFile = "/tmp/projekt";
    mkfifo(fifoFile, 0666);

    int fd2[2];
    if(pipe(fd2)) {
        fprintf(stderr, "Error creating pipe");
        return 1;
    }

    if (!fork()) {
        pids[0] = getpid();
        char x;
        fd1 = open(fifoFile, O_WRONLY);
        while (read(STDIN_FILENO, &x, 1)) {
            write(fd1, &x, sizeof(x));
        }
        close(fd1);
        return 0;
    }

    if (!fork()) {
        pids[1] = getpid();
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
        pids[2] = getpid();
        close(fd2[1]);
        char x;
//        fprintf(stdout, "Proces 3 odebrał:\n");
        while (read(fd2[0], &x, 1)) {
//            write(STDOUT_FILENO, &x, sizeof(x));
        }
        close(fd2[0]);
        return 0;
    }


    return 0;
}