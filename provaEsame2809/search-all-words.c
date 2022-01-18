//soluzione con coda di messaggi, con n processi senza limite su n

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#define MAX_DIM_ROW 2048
#define EMPTY 0 
#define FULL 1
#define MUTEX 2
const char* END_MESS = "end||end";
int tot_child = 0;

typedef struct msg {
    long type; // da 1 n sono da e per i processi Wi (i da 1 a n)
    char message;
} MSG;

char strsearch(const char *buffer, const char* mystring){
    char* ptr;

    if((ptr = strcasestr(buffer, mystring)) == NULL){
        return 'F';
    }else{
        if(ptr == buffer){
            if(*(ptr + strlen(mystring)) == ' ' || 
                *(ptr + strlen(mystring)) == '\n' ||
                  *(ptr + strlen(mystring)) == '\r' ||
                    *(ptr + strlen(mystring)) == ',' ||
                      *(ptr + strlen(mystring)) == ';') return 'T';
            else return 'F';
        }else{
            if(*(ptr-1) != ' ') return 'F';
            if(*(ptr + strlen(mystring)) == ' ' || 
                        *(ptr + strlen(mystring)) == '\n' ||
                            *(ptr + strlen(mystring)) == '\r' ||
                                *(ptr + strlen(mystring)) == ',' ||
                                    *(ptr + strlen(mystring)) == ';') return 'T';
            else return 'F';
        }
    }
}

int WAIT(int sem_id, int num_sem, int n){
    struct  sembuf op[1] = {{num_sem, -n, 0}};
    return semop(sem_id, op, 1);
    
}

int SIGNAL(int sem_id, int num_sem, int n){
    struct  sembuf op[1] = {{num_sem, +n, 0}};
    return semop(sem_id, op, 1);
}

int r_utility_count_match(int msg_id, int w_num, struct msg *snd_message, struct msg* rcv_message){
    //blocco che invia messaggi ai vari processi W
    int match_count = 0;
    for(int i = 1; i <= w_num; i ++){
        snd_message->type = (long)i;
        snd_message->message = 'R';
        msgsnd(msg_id, snd_message, sizeof(char), 0);
    }

    for(int i = 1; i <= w_num; i ++){
        msgrcv(msg_id, rcv_message, sizeof(char), i + tot_child, 0); //rcv da tuti i w
        if(rcv_message->message == 'T') match_count ++;
    }

    return match_count;
}

void r_process_routine(FILE *file_name, int w_num, int shm_id, int sem_id, int pipe_fd[2], int msg_id){
    FILE *file_fd = file_name;
    char * row;
    char string[MAX_DIM_ROW];
    struct msg rcv_ptr; //messaggio da ricevere
    struct msg snd_ptr; //messaggio da mandare

    struct msg *rcv_message = &rcv_ptr; //messaggio da ricevere
    struct msg *snd_message = &snd_ptr; //messaggio da mandare
    int match_count = 0;

    //fare la shmat per l'attach della shared memory
    if((row = (char *)shmat(shm_id, NULL, 0)) == (char *)-1){
        perror("shmat ");
        exit(1);
    }

    close(pipe_fd[0]);

    //copio una riga per volta
    while(feof(file_fd) == 0){
        WAIT(sem_id, EMPTY, w_num);
        WAIT(sem_id, MUTEX, 1);
            fgets(row, MAX_DIM_ROW, file_fd);
        SIGNAL(sem_id, MUTEX, 1);
        SIGNAL(sem_id, FULL, w_num);

        //blocco che invia messaggi ai vari processi W
        match_count = r_utility_count_match(msg_id, w_num, snd_message, rcv_message);

        if(match_count == w_num){
            strcpy(string, row);
            write(pipe_fd[1], string, MAX_DIM_ROW);
        }

        match_count = 0;
    }
    
    WAIT(sem_id, EMPTY, w_num);
    WAIT(sem_id, MUTEX, 1);
        strcpy(row, END_MESS);
    SIGNAL(sem_id, MUTEX, 1);
    SIGNAL(sem_id, FULL, w_num);

    //blocco che invia messaggi ai vari processi W
    match_count = r_utility_count_match(msg_id, w_num, snd_message, rcv_message);

    strcpy(string, row);
    write(pipe_fd[1], string, MAX_DIM_ROW);
}

void o_process_routine(int *pipe_fd){
    char str[MAX_DIM_ROW];
    close(pipe_fd[1]);

    while(1){
        if(read(pipe_fd[0], str, MAX_DIM_ROW) == -1){
            perror("read ");
            exit(1);
        }
        if(strcmp(str, END_MESS) == 0) return;
        printf("[...] %s", str);

    }
}

void w_process_routine(int n, int shm_id, int sem_id, int msg_id, char * my_string){
    fflush(stdout);
    char * row;
    char buffer[MAX_DIM_ROW];
    struct msg rcv_message; //messaggio da ricevere
    struct msg snd_message; //messaggio da mandare
    snd_message.type = (long)n + tot_child;

    //fare la shmat per l'attach della shared memory
    if((row = (char *)shmat(shm_id, NULL, 0)) == (char *)-1){
        perror("shmat ");
        exit(1);
    }
    
    //copio una riga per volta
    while(1){ 
        msgrcv(msg_id, &rcv_message, sizeof(char), (long)n, 0);//(tipo n) il processo w si blocca se non è arrivato un messaggio per lui

        WAIT(sem_id, FULL, 1);
        WAIT(sem_id, MUTEX, 1);//sistemare i semafori
        strcpy(buffer, row);
        SIGNAL(sem_id, MUTEX, 1);
        SIGNAL(sem_id, EMPTY, 1);

        //blocco sul controllo della stringa per determinare se la parola è presente
        snd_message.message = strsearch(buffer, my_string);

        msgsnd(msg_id,  &snd_message, sizeof(char), 0);
        
        if(strcmp(buffer, END_MESS) == 0 ) return;
    }

}

int main(int argc, char * argv[]){
    int i, n; 
    int shm_id;
    int pipe_fd[2];
    int mutex_shm;
    int msg_id;
    FILE *file_id;

    union semun{
        unsigned short array[3];
    };

    if(argc < 3){
        printf("usage : ./search-all-words <text-file> <word-1> [<word-2>] ... [<word-n>]");
        exit(1);
    }

    //n >= 3
    n = argc; // = 2 + M parole da cercare
    tot_child = n - 2;
    //apro il file da cui leggere le righe
    if((file_id = fopen(argv[1], "r")) == (FILE *)-1){
        perror("fopen ");
        exit(1);
    }

    //creiamo il segmento di memoria condivisa
    if((shm_id = shmget(IPC_PRIVATE, MAX_DIM_ROW, IPC_CREAT | 0600)) == -1){
        perror("shmget ");
        exit(1);
    }

    //creazione di un set di semafori, 3 per la precisione 
    if((mutex_shm = semget(IPC_PRIVATE, 3, IPC_CREAT | 0600)) == -1){
        perror("semget ");
        exit(1);
    }

    if((msg_id = msgget(IPC_PRIVATE,IPC_CREAT | 0600)) == -1){
        perror("msget ");
        exit(1);
    }

    semctl(mutex_shm, EMPTY, SETVAL,  n-2 );
    semctl(mutex_shm, FULL, SETVAL,  0);
    semctl(mutex_shm, MUTEX, SETVAL,  1);

    //inizializzo la pipe
    if(pipe(pipe_fd) == -1){
        perror("pipe ");
        exit(1);
    }

    //il processo padre crea n = 2 + M figli ( dove M è il numero di parole da cercare
    for(i = 0; i < n; i ++){

        if(i == 0){
            if(fork() == 0){
                r_process_routine(file_id, n -2, shm_id, mutex_shm, pipe_fd, msg_id);
                exit(0);
            }
        }else if(i == 1){
            if(fork() == 0){
                o_process_routine(pipe_fd);
                exit(0);
            }
        }else if(fork() == 0){
            w_process_routine(i-1, shm_id, mutex_shm, msg_id, argv[i]);
            exit(0);
        }

    }

    for(int j = 0; j < n; j++)
        wait(NULL);

    fflush(stdout);
    
    printf("Processo padre terminato con successo!\n");
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(mutex_shm, 0, IPC_RMID);
    msgctl(msg_id,IPC_RMID, NULL);
    return 0;
}