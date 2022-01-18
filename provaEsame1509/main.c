#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_LEN_STR 1024
#define MAX_LEN_CONTENT 2048
#define IN 0
#define DB 1
#define DB1 2
#define OUT 3

char *end = " ";

typedef struct{
    char str[MAX_LEN_STR];
    int process;
} msg;

typedef struct{
    char str[MAX_LEN_STR];
    int process;
    int valore;
} msgOUT;

typedef struct{
    char nome[MAX_LEN_STR];
    int valore;
} query;

int WAIT(int sem_id, int sem_num, int n){
    struct sembuf op[1] = {{sem_num, -n, 0}};
    return semop(sem_id, op, 1);
}

int SIGNAL(int sem_id, int sem_num, int n){
    struct sembuf op[1] = {{sem_num, n, 0}};
    return semop(sem_id, op, 1);
}

int is_present(query *database, int len, char *str){
    for(int i = 0; i < len; i++){
        if(strcmp(database[i].nome,str) == 0){
            return database->valore;
        }
    }

    return -1;
}

void IN_process_routine(int pid, char * nome_file_query, int shm_id, int sem_id){
    FILE *file_id;
    msg *shm;
    char str[MAX_LEN_STR];
    int index = 0;
    
    //attach mem condivisa
    if((shm = (msg*) shmat(shm_id, NULL, 0)) == (msg *)-1){
        perror("shmat IN: ");
        exit(1);
    }
    
    //open File
    if((file_id = fopen(nome_file_query, "r")) == (FILE*)-1){
        perror("fopen: ");
        exit(1);
    }

    while(fgets(str, MAX_LEN_STR, file_id)){
        str[strlen(str)-1] = '\0';
        WAIT(sem_id, DB, 1);
            strcpy(shm->str, str);
            shm->process = pid;
        SIGNAL(sem_id, IN, 1);

        index++;
        printf("IN-%d: inviata query n.%d '%s'\n", pid, index, str);
    }

    WAIT(sem_id, DB, 1);
        strcpy(shm->str, end);
        shm->process = pid;
    SIGNAL(sem_id, IN, 1);
}

void DB_process_routine(char * database_file_name, int shm_id, int shmOUT_id, int sem_id, int numb_process ){
    FILE* file_id;
    msg * shm;
    msgOUT * shmOUT;
    int num_rows = 0;
    char str[MAX_LEN_CONTENT];
    char val[MAX_LEN_STR];
    int process_id;
    int num_char = 0;
    int valore = 0;
    int numb_ret = 0;
    //attach shm
    if((shm = (msg*) shmat(shm_id, NULL, 0 )) == (msg*) -1){
        perror("shmat DB-IN: ");
        exit(1);
    }

    if((shmOUT = (msgOUT*) shmat(shmOUT_id, NULL, 0 )) == (msgOUT*) -1){
        perror("shmat DB-OUT: ");
        exit(1);
    }

    //fopen
    if((file_id = fopen(database_file_name, "r")) == (FILE*) -1){
        perror("fopen DB: ");
        exit(1);
    }

    for(char c = fgetc(file_id); c != EOF; c = fgetc(file_id))
        if(c = '\n')
            num_rows ++;

    fclose(file_id);
    //alloco un array di struct
    query database[num_rows];

    //fopen
    if((file_id = fopen(database_file_name, "r")) == (FILE*) -1){
        perror("fopen DB: ");
        exit(1);
    }

    int j = 0;
    while(fgets(str, MAX_LEN_CONTENT, file_id)){
        for(int i = 0; i < strlen(str); i++){
            if(str[i] == ':'){
                num_char = i;
                break;
            }
        }

        strncpy(database[j].nome, str, num_char);

        strncpy(val, &str[num_char], strlen(str)-1 - num_char);

        database[j].valore = atoi(val);
        
        printf("DB: inserito in db (%s, %d)\n", database[j].nome,database[j].valore );

        j++;
    }

    while(1){
        WAIT(sem_id, IN, 1);
            strcpy(str, shm->str);
            process_id = shm->process;
        SIGNAL(sem_id, DB, 1);

        //richiama metodo ricerca
        if((valore = is_present(database, num_rows, str) )!= -1){
            WAIT(sem_id, OUT, 1);
                shmOUT->process = process_id;
                strcpy(shmOUT->str, str);
                shmOUT->valore = valore;
            SIGNAL(sem_id, DB1, 1);
            printf("DB: query '%s' da IN-%d trovata con valore %d\n", str, process_id, valore);
        }else
            printf("DB: query '%s' da IN-%d non trovata\n",str, process_id);

        if(strcmp(str, end) == 0){
            numb_ret ++;
        }

        if(numb_ret == numb_process)
            return;

    }
}

void OUT_process_routine(int sem_id, int shm_id, int num_process){
    //attach shm
    msgOUT * shm;
    char str[MAX_LEN_STR];
    int valore = 0;
    int pid;
    int totali[num_process];
    int num_valori[num_process];

    for(int i = 0; i < num_process; i++){
        num_valori[i] = 0;
    }

    if((shm = (msgOUT*)shmat(shm_id, NULL, 0)) == (msgOUT *)-1){
        perror("shmat OUT: ");
        exit(1);
    }
    
    while(1){
        WAIT(sem_id, DB1, 1);
            pid = shm->process;
            valore = shm->valore;
            strcpy(str, shm->str);
        SIGNAL(sem_id, OUT, 1);

        totali[pid-1] += valore;
        num_valori[pid-1] +=1;

        if(str == " "){
            for(int i = 0; i < num_process; i++){
                printf("OUT: ricevuti n.%d valori validi per IN-%d con totale %d\n", num_valori[i], i+1, totali[i]);
            }
            return;
        }
    }
    
}

int main(int argc, char *argv[]){
    int sem_id, shmIN_DB_id, shmDB_OUT_id;

    if(argc < 3){
        printf("usage: %s <db-file> <query-file-1> ... [<query-file-n>]\n",
                 argv[0]);
        exit(0);
    }

    if((sem_id = semget(IPC_PRIVATE, 4, IPC_CREAT | 0660)) == -1){
        perror("semget: ");
        exit(1);
    }

    if((shmIN_DB_id = shmget(IPC_PRIVATE, sizeof(msg) , IPC_CREAT | 0660)) == -1){
        perror("semget IN-DB: ");
        exit(1);
    }

    if((shmDB_OUT_id = shmget(IPC_PRIVATE, sizeof(msgOUT), IPC_CREAT | 0660)) == -1){
        perror("semget DB-OUT: ");
        exit(1);
    }

    if(semctl(sem_id, IN, SETVAL, 0)){
        perror("semctl IN: ");
        exit(1);
    }

    if(semctl(sem_id, DB, SETVAL, 1)){
        perror("semctl DB: ");
        exit(1);
    }

    if(semctl(sem_id, DB1, SETVAL, 0)){
        perror("semctl DB: ");
        exit(1);
    }

    if(semctl(sem_id, OUT, SETVAL, 1)){
        perror("semctl DB: ");
        exit(1);
    }

    for(int i = 0 ; i < argc; i++){
        if( i == 0){
            if(fork() == 0){
                DB_process_routine(argv[i], shmIN_DB_id, shmDB_OUT_id, sem_id, argc -2 );
            }
        }

        if( i == 1){
            if(fork() == 0){
                OUT_process_routine(sem_id, shmDB_OUT_id, argc - 2);
            }
        }

        if( i > 1){
            if(fork() == 0){
                IN_process_routine(i - 1,argv[i], shmIN_DB_id, sem_id);
            }
        }
    }

    for(int i = 0 ; i < argc ; i ++){
        wait(NULL);
    }


    if(semctl(sem_id, IN, IPC_RMID,0) == -1){
        perror("semctl IN RMID: ");
        exit(1);
    }
    if(semctl(sem_id, DB, IPC_RMID, 0) == -1){
        perror("semctl DB RMID: ");
        exit(1);
    }
    if(semctl(sem_id, DB1, IPC_RMID,0) == -1){
        perror("semctl DB1 RMID: ");
        exit(1);
    }
    if(semctl(sem_id, OUT, IPC_RMID, 0) == -1){
        perror("semctl OUT RMID: ");
        exit(1);
    }
    if(shmctl(shmIN_DB_id, IPC_RMID, NULL) == -1){
        perror("shmctl IN-DB RMID: ");
        exit(1);
    }
    if(shmctl(shmDB_OUT_id, IPC_RMID, NULL) == -1){
        perror("shmctl DB-OUT RMID: ");
        exit(1);
    }

    return 0;

}
