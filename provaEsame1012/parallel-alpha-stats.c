#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <ctype.h>

#define ALPHA_COUNT 26  //Numero di lettere dell'alfabeto, numero di processi L
#define MAX_DIM_ROW 2048
#define L 0
#define S 1
#define END_STR "\t\n--END_MSG--\t\n"

typedef struct msg {
    long type; // da 1 a 26 sono da e per i processi Wi (i da 1 a n)
    char process_char;
    int char_count;
} MSG;

int WAIT(int sem_id, int num_sem, int n){
    struct  sembuf op[1] = {{num_sem, -n, 0}};
    return semop(sem_id, op, 1);
    
}

int SIGNAL(int sem_id, int num_sem, int n){
    struct  sembuf op[1] = {{num_sem, +n, 0}};
    return semop(sem_id, op, 1);
}

int get_occurency(char *string, char process_char){
    //ricerca case insensitive
    int str_dim = strlen(string);
    int char_count = 0;

    for(int i = 0; i < str_dim; i++){
        if(tolower(string[i]) == process_char){
            char_count++;
        }
    }
    return  char_count;
}

void utility_routine(int shm_id, int sem_id, int msg_id){
    //distruzione ipc structures
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);
}

void s_process_routine(int msg_id,int sem_id){
    /*
        1)  colleziona le info statistiche dai 26 processi L

    */
   int info_stats[ALPHA_COUNT] = {0};
   int info_stats_global[ALPHA_COUNT] = {0};
    struct msg rcv_message; 
    struct msg snd_message;
    int j = 1;
    while(j){
        //2)  invia i msg ai processi L
        WAIT(sem_id, L, 1);
        for(int i = 0; i < ALPHA_COUNT; i++)
        {
            snd_message.type = (long)(i + 1);
            msgsnd(msg_id, &snd_message, sizeof(char) + sizeof(int), 0);
        }
            
            

        for(int i = 1; i <= ALPHA_COUNT; i++)
        {
            msgrcv(msg_id, &rcv_message, sizeof(char) + sizeof(int), ALPHA_COUNT + i, 0);
            info_stats[i-1] = rcv_message.char_count;
            info_stats_global[i-1] += info_stats[i-1];

            if(rcv_message.process_char == 'F')
            {
                j = -1;
            }else{
                if(i==1){
                    printf("\n[S] riga n.%d: ", j);
                }
                printf("%c=%d ", (char)((i-1) + 'A') , info_stats[i-1]);
            }
                
            fflush(stdout);
        }

            printf("\n");

        SIGNAL(sem_id, S, 1);
        j++;
    }

    printf("[S] intero file: ");
    for(int i = 0; i < ALPHA_COUNT; i++){
        printf("%c=%d ", (char)((i) + 'A') , info_stats_global[i]);
    }
    
    printf("\n");
}

void l_process_routine(int char_id, int shm_id, int msg_id, int sem_id)
{
   char *buffer;

   //1) settare lettera del processo i
   char process_char = char_id + 'a';

    struct msg  rcv_message; 
    struct msg  send_message;

    send_message.type = (long)(char_id + 1 + ALPHA_COUNT);
    send_message.process_char = process_char;

   //2)  attach della shm
    if((buffer = (char *)shmat(shm_id, NULL, 0)) == (char *)-1){
        perror("shmat: ");
        exit(1);
    }

    while(1){
        //aspettare il msg dal processo S
        msgrcv(msg_id, &rcv_message, sizeof(char) + sizeof(int), (long)(char_id + 1), 0);
        //controllo fine documento
        if(strcmp(buffer, END_STR) == 0){
            send_message.char_count = 0;
            send_message.process_char = 'F';
            msgsnd(msg_id, &send_message, sizeof(char) + sizeof(int), 0);
            return;
        }

        //aspettare sul semaforo
        WAIT(sem_id, L, 1);

        //costruire msg per processo S
        send_message.char_count = get_occurency(buffer, process_char);

        //invio msg -> (process_char, char_count) a processo S
        msgsnd(msg_id, &send_message, sizeof(char) + sizeof(int), 0);
    }
    
}

int main(int argc, char* argv[]){
    
    /*
        blocco di creazione delle strutture di ipc
    */
   int shm_id, sem_id, msg_id;
    FILE* file_id;
   //controlliamo se sono stati passati solo due argomenti
   if(argc != 2)
   {
       printf("Usage:   ./parallel-alpha-stats <text-file>");
       exit(0);
   }

   //apriamo il file di testo sul quale va fatto il parsing
   if((file_id = fopen(argv[1], "r")) == (FILE *)-1)
    {
        perror("fopen ");
        exit(1);
    }

   //creiamo il segmento di memoria condivisa
    if((shm_id = shmget(IPC_PRIVATE, MAX_DIM_ROW, IPC_CREAT | 0660)) == -1)
    {
        perror("shmget: ");
        exit(1);
    }

    //creazione di un set di semafori, 3 per la precisione 
    if((sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0660)) == -1)
    {
        perror("semget ");
        exit(1);
    }

    semctl(sem_id, L, SETVAL, 0);
    semctl(sem_id, S, SETVAL, 0);

    //Creazione di una coda di messaggi
    if((msg_id = msgget(IPC_PRIVATE,IPC_CREAT | 0660)) == -1)
    {
        perror("msget ");
        exit(1);
    }

    for (size_t i = 0; i < ALPHA_COUNT; i++)
    {
        if(fork() == 0)             //vengono creati i 26 processi L
        {
            l_process_routine(i, shm_id, msg_id, sem_id);   //la funzione prende come parametro l'indice i
            exit(0);                        //che corrisponderà ad una lettere dell'alfabeto
        }                          
    }

    if(fork() == 0){
        s_process_routine(msg_id, sem_id);
        exit(0);
    }
    

    int i = 1;
    //char riga[MAX_DIM_ROW];
    char *shmem;

    //attach memoria condivisa
    if((shmem = (char *)shmat(shm_id, NULL, 0)) == (char *)-1){
        perror("shmat: ");
        utility_routine(shm_id, sem_id, msg_id);
        exit(1);
    }

    while(fgets(shmem, MAX_DIM_ROW, file_id))
    {   
        printf("[P] riga n.%d: %s", i, shmem);
        fflush(stdout);
        i++;

        //risvegliare i processi L-i
        SIGNAL(sem_id, L, ALPHA_COUNT+1);

        WAIT(sem_id, S, 1); //forse da settare a ALPHA_COUNT
    }
    printf("\n");
    strcpy(shmem, END_STR);

    SIGNAL(sem_id, L, ALPHA_COUNT+1);

    for(int i = 0; i < ALPHA_COUNT +1; i++){
        wait(NULL);
    }

    //distruzione ipc structures
    shmctl(shm_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    msgctl(msg_id, IPC_RMID, NULL);

    fflush(stdout);
    printf("Processo padre finito correttamente\n");

    return 0;
}