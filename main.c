// Jakub Karczewski I7Y2S1
// Numer 13
// Si1 := SIGUSR1
// Si2 := SIGINT
// Si3 := SIGCONT
// Si4 := SIGUSR2


#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>

volatile int *pids;

void propagate_signal(int sig) {
    for(int i=0; i<3; i++) {
        if(pids[i] != getpid()) {
            union sigval s;
            s.sival_int = sig;
//            printf("I try to send to %d signal: %d\n", pids[i], sig);
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
//            printf("%d Ending\n", getpid());
            exit(0);
        case SIGINT:
//            printf("%d Pausing\n", getpid());
            fflush(stdout);

            sigset_t set;
            sigfillset(&set);
            sigdelset(&set, SIGCONT);
            sigdelset(&set, SIGUSR2);
            sigdelset(&set, SIGUSR1);
            sigsuspend(&set);

            break;
        case SIGCONT:
//            printf("%d Resuming\n", getpid());
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

    // Mechanizm komunikacji 1: kolejka FIFO
    int fd1;
    char *fifoFile = "/tmp/projekt";
    mkfifo(fifoFile, 0666);

    // Mechanizm komunikacji 2: plik
    int fd2[2];
    if(pipe(fd2)) {
        fprintf(stderr, "Error creating pipe");
        return 1;
    }

    // Proces 1: czyta dane ze standardowego strumienia wejsciowego i przekazuje je w niezmienionej formie do procesu 2
    if (!fork()) {
        pids[0] = getpid();
        fd1 = open(fifoFile, O_WRONLY);
        char x;
        if(isatty(fileno(stdin))) {
            char buff[256];
            int loop = 1;
            do {
                fgets(buff, sizeof(buff), stdin);
                for (int i = 0; i < 256; i++) {
                    if (buff[i] == '\n') {
                        loop = 0;
                        break;
                    } else {
                        write(fd1, &buff[i], sizeof(buff[i]));
                    }
                }
            } while (loop);
        } else {
            while (read(STDIN_FILENO, &x, 1)) {
                write(fd1, &x, sizeof(x));
            }
        }

        close(fd1);
        return 0;
    }

    // Proces 2: pobiera dane przeslane przez proces 1. Oblicza ilosc odczytanych bajtow, wyswietla ja na standardowym strumiwniu diagnostycznym,
    // a nastepnie przekazuje odebrane dane mechanizmem komunikacji do procesu 3
    if (!fork()) {
        pids[1] = getpid();
        close(fd2[0]);

        fd1 = open(fifoFile, O_RDONLY);

        char x;
        unsigned long long count = 0;
        while (read(fd1, &x, 1)) {
            write(fd2[1], &x, sizeof(x));
            count++;
        }
        unlink(fifoFile);
        close(fd2[1]);
        fprintf(stderr, "Proces 2 odczytał: %lld bytes\n", count);
        return 0;
    }


    // Proces 3: pobiera dane wyprodukowane przez proces 2 i umieszcza je w standardowym strumieniu wyjsciowym.
    if(!fork()) {
        pids[2] = getpid();
        close(fd2[1]);

        char x;
        fprintf(stdout, "Proces 3 odebrał:\n");
        while (read(fd2[0], &x, 1)) {
            fprintf(stdout, "%c", x);
        }
        close(fd2[0]);
        return 0;
    }

    // Wyczekuj na dziecko w trybie uzytkownika
    // Niezgodne z poleceniem, stosowane w celu poprawnego dzialania terminalu
    if(isatty(fileno(stdin)))
        wait(NULL);

    return 0;
}