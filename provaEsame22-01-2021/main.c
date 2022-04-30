#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>

#define MAX_LEN 1024
#define MAIUSCOLO 0
#define MINUSCOLO 1
#define SOSTITUISCI 2

int WAIT(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num, -1,0}};
    return semop(sem_id, op, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num, 1,0}};
    return semop(sem_id, op, 1);
}


char * process_filter(char* filter, char* str, int filter_type){
    char filtro[128];
    strcpy(filtro, &filter[1]);
    fflush(stdout);
    fflush(stdout);

    char *new_parola;

    if(filter_type == MAIUSCOLO){
        while((new_parola = strstr(str, filtro)) != NULL){
            for(int i = 0; i < strlen(filtro); i++){
                new_parola[i] = toupper(new_parola[i]);
            }
        }
        return str;
    }else if(filter_type == MINUSCOLO){
        while((new_parola = strstr(str, filtro)) != NULL){
            for(int i = 0; i < strlen(filtro); i++){
                new_parola[i] = tolower(new_parola[i]);
            }
        }
        return str;
    }else if(filter_type == SOSTITUISCI){
        char parola1[128];
        char parola2[128];
        char substr[128];
        sscanf(filter,"%%%[^,],%s", parola1, parola2);

        while((new_parola = strcasestr(str, parola1)) != NULL){
            if((strlen(str)-strlen(parola1)+strlen(parola2)) > MAX_LEN){
                return NULL;
            }
            strcpy(substr, &new_parola[strlen(parola1)]);
            new_parola[0] = '\0';
            strcat(new_parola, parola2);
            strcat(new_parola, substr);
        }
        return str;
    }

    return NULL;
}


void F_process_routine(char * filter, int sem_id, int shm_id, int pid, int next_process){
    char * frase;
    int filter_type = 0;
    int index = 0;
    if((frase = (char*)shmat(shm_id, NULL, 0)) == (char*)-1){
        perror("shmat: ");
        exit(1);
    }

    if(strlen(filter) > 1){
        if(filter[0] == '^'){
            //^parola -->cerca la parola e la trasforma in maiuscolo
            filter_type = MAIUSCOLO;         
        }else if(filter[0] == '_'){
            //_parola -->cerca la parola e la trasforma in minuscolo
            filter_type = MINUSCOLO;         
        }else if(filter[0] == '%'){
            //%parola1,parola2 -->cerca la parola1 e la sostiuisce con parola2
            filter_type = SOSTITUISCI;         
        }
    }else{
        perror("FIltro non riconosciuto: ");
        exit(1);
    }

    while(1){
        index++;
        WAIT(sem_id, pid);
        if(strcmp(frase, "\\end\\") == 0){
            SIGNAL(sem_id, next_process);
            return;
        }
        strcpy(frase, process_filter(filter, frase, filter_type));
        SIGNAL(sem_id, next_process);
    }
}

int main(int argc, char* argv[]){
    if(argc < 3){
        printf("Usage: %s <file.txt> <filter-1> [filter-2] [...]\n", argv[0]);
        exit(0);
    }

    int shm_id, sem_id;
    FILE* file_id;
    int next_process = 0;

    if((shm_id = shmget(IPC_PRIVATE, MAX_LEN, IPC_CREAT | 0660)) == -1){
        perror("shmget: ");
        exit(1);
    }

    if((sem_id = semget(IPC_PRIVATE, argc-1, IPC_CREAT | 0660)) == -1){
        perror("semget: ");
        exit(1);
    }

    if(semctl(sem_id, argc-2, SETVAL, 1) == -1){
        perror("semctl: ");
        exit(1);
    }

    for(int i = 0; i < argc-2; i++){
        if(semctl(sem_id, i, SETVAL, 0) == -1){
            perror("semctl: ");
            exit(1);
        }
    }

    if((file_id = fopen(argv[1], "r")) == (FILE*)-1){
        perror("fopen: ");
        exit(1);
    }

    for(int i = 0; i < argc-2; i++){
        if(fork() == 0) {
            F_process_routine(argv[i+2], sem_id, shm_id, i,i+1);
            exit(0);
        }
    }

    char *map;
    char frase[MAX_LEN];
    if((map = (char*)shmat(shm_id, NULL, 0)) == (char*)-1){
        perror("shmat: ");
        exit(1);
    }

    //codice
    WAIT(sem_id, argc-2);
    while(fgets(frase, MAX_LEN, file_id)){
            strcpy(map, frase);
            SIGNAL(sem_id, next_process);
            WAIT(sem_id, argc-2);
            printf("%s", map);
    }

    strcpy(map, "\\end\\");
    SIGNAL(sem_id, next_process);

    for(int i = 0; i < argc-2; i++){
        wait(NULL);
    }

    if(semctl(sem_id, 0, IPC_RMID) == -1){
        perror("sem IPCRMID: ");
        exit(1);
    }
    
    if(shmctl(shm_id, IPC_RMID, NULL) == -1){
        perror("semctl: ");
        exit(1);
    }

}