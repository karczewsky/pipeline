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
#include <pthread.h>

typedef struct
{
    int bytes_left;
    pthread_mutex_t mutex_write;
    pthread_mutex_t mutex_read;
} shared_data;

static shared_data* data = NULL;

volatile int *pids;


void initialise_shared()
{
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_SHARED | MAP_ANONYMOUS;
    data = mmap(NULL, sizeof(shared_data), prot, flags, -1, 0);

    data->bytes_left = 0;

    pthread_mutexattr_t attr_read; pthread_mutexattr_t attr_write;
    pthread_mutexattr_init(&attr_read); pthread_mutexattr_init(&attr_write);
    pthread_mutexattr_setpshared(&attr_read, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setpshared(&attr_write, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&data->mutex_read, &attr_read);
    pthread_mutex_init(&data->mutex_write, &attr_write);
    pthread_mutex_lock(&data->mutex_read);
}


void propagate_signal(int sig) {
    for(int i=0; i<3; i++) {
        if(pids[i] != getpid()) {
            union sigval s;
            s.sival_int = sig;
            sigqueue(pids[i], SIGUSR2, s);
        }
    }
    fflush(stdout);
}

void sighandler(int sig, siginfo_t *info, void *context) {
    if(sig == SIGUSR2)
        sig = info->si_value.sival_int;
    else
        propagate_signal(sig);


    switch (sig) {
        // END
        case SIGUSR1:
            exit(0);
        // PAUSE
        case SIGINT:
            fflush(stdout);

            sigset_t set;
            sigfillset(&set);
            sigdelset(&set, SIGCONT);
            sigdelset(&set, SIGUSR2);
            sigdelset(&set, SIGUSR1);
            sigsuspend(&set);

            break;
        // RESUME
        case SIGCONT:
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


    // Mechanizm komunikacji 1: Kolejka FIFO
    int fd1;
    char *fifoFile = "./projekt_fifo";
    mkfifo(fifoFile, 0666);

    // Mechanizm komunikacji 2: Plik
    FILE *fd2;
    char *mutexFile = "./projekt_mutex";
    initialise_shared();


    // Proces 1: czyta dane ze standardowego strumienia wejsciowego i przekazuje je w niezmienionej formie do procesu 2
    if (!fork()) {
        pids[0] = getpid();
        fd1 = open(fifoFile, O_WRONLY);
        char x;
        if(isatty(fileno(stdin))) {
            printf("Podaj ciag znakow do odczytania przez proces 1: ");
            char buff[256];
            int loop = 1;
            while (loop) {
                fgets(buff, sizeof(buff), stdin);
                for (int i = 0; i < 256; i++) {
                    if (buff[i] == '\n') {
                        loop = 0;
                        break;
                    } else {
                        write(fd1, &buff[i], sizeof(buff[i]));
                    }
                }
            }
        } else {
            while (read(STDIN_FILENO, &x, 1)) {
                write(fd1, &x, sizeof(x));
            }
        }

        close(fd1);
        return 0;
    }

    // Proces 2: pobiera dane przeslane przez proces 1.
    //           Oblicza ilosc odczytanych bajtow, wyswietla ja na standardowym strumieniu diagnostycznym,
    //           a nastepnie przekazuje odebrane dane mechanizmem komunikacji do procesu 3
    if (!fork()) {
        pids[1] = getpid();

        fd1 = open(fifoFile, O_RDONLY);

        char x;
        unsigned long long count = 0;
        fd2 = fopen(mutexFile, "w");
        if (fd2 == NULL) {
            fprintf(stderr, "PROC 2: error opening communication file");
            return 1;
        }

        while (read(fd1, &x, 1)) {
            pthread_mutex_lock(&data->mutex_write);

            fprintf(fd2, "%c", x);
            fflush(fd2);
            data->bytes_left++;
            count++;
            fprintf(stdout, "PROC 2 | Bytes left: %d\n", data->bytes_left);
            pthread_mutex_unlock(&data->mutex_read);
        }
        pthread_mutex_unlock(&data->mutex_read);
        unlink(fifoFile);
        fprintf(stderr, "\nProces 2 odczytał: %lld bytes\n", count);
        return 0;
    }


    // Proces 3: pobiera dane wyprodukowane przez proces 2 i umieszcza je w standardowym strumieniu wyjsciowym.
    if(!fork()) {
        pids[2] = getpid();

        fd2 = fopen(mutexFile, "r");
        if (fd2 == NULL) {
            fprintf(stderr, "PROC 3: error opening communication file");
            return 1;
        }

        char x;
        int loop = 1;
        int first_iter = 1;
        while (loop)
        {
            pthread_mutex_lock(&data->mutex_read);
            if (first_iter) {
                first_iter = 0;
                fprintf(stdout, "Proces 3 odebrał:\n");
            }


            if (data->bytes_left > 0) {
                x = (char)fgetc(fd2);
//                printf("%c", x);

                fflush(stdout);
                data->bytes_left--;
                fprintf(stdout, "PROC 3 | Bytes left: %d\n", data->bytes_left);
                pthread_mutex_unlock(&data->mutex_write);
            } else {
                printf("Koniec %d", data->bytes_left);
                fflush(stdout);
                loop = 0;
            }
        }

        unlink(mutexFile);
        return 0;
    }

    // Wyczekuj na dziecko w trybie uzytkownika
    // Niezgodne z poleceniem, stosowane w celu poprawnego dzialania terminalu
    if(isatty(fileno(stdin)))
        wait(NULL);

    return 0;
}