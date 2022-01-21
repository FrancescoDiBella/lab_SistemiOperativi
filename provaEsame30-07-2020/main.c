#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN 64
#define P 0
#define R 1
#define L 2

int WAIT(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num, -1,0}};
    return semop(sem_id, op, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num, 1,0}};
    return semop(sem_id, op, 1);
}

int is_palindroma(char *str){
    int len = strlen(str);
    int count = 0;

    for(int i = 0; i < len/2; i++){
        if(tolower(str[i]) == tolower(str[len -1 -i])){
            count++;
        }
    }

    if(count == len/2)
        return 1;
    else return 0;
}

void R_process_routine(int argc, char* file_name, int sem_id, int shm_id){
    char * shm;
    if((shm = (char*)shmat(shm_id, NULL, 0)) == (char*)-1){
        perror("shmat: ");
        exit(1);
    }

    if(argc == 1){
        //////////////da input
        char parola[MAX_LEN];

        while(1){
            printf("Digita la parola da controllare:\n----  ");
            scanf("%s", parola);

            
            //metti in mem condivisa
            strcpy(shm, parola);
            SIGNAL(sem_id, 0);
            if(strcmp(parola, "\\logout\\") == 0){
                break;
            }
            WAIT(sem_id, 1);
        }
    }else if(argc == 3){
        //////////////da file
        char parola[MAX_LEN];
        FILE * file_id;

        if((file_id = fopen(file_name, "r")) == (FILE*)-1){
            perror("fopen: ");
            exit(1);
        }

        while(fgets(parola, MAX_LEN, file_id)){
            sscanf(parola, "%[^\n]\n", parola);
            printf("Controllo per la parola %s:----\n", parola);            
            //metti in mem condivisa
            strcpy(shm, parola);
            SIGNAL(sem_id, 0);
            WAIT(sem_id, 1);
        }
        strcpy(parola, "\\logout\\");
        strcpy(shm, parola);
        SIGNAL(sem_id, 0);
        fclose(file_id);

    }
}

void L_process_routine(int argc, char* file_name, int sem_id, int shm_id){
    char * shm;
    if((shm = (char*)shmat(shm_id, NULL, 0)) == (char*)-1){
        perror("shmat: ");
        exit(1);
    }


    if(argc == 1){
        //////////////da input
        while(1){
            WAIT(sem_id, L);
                if(strcmp(shm, "\\logout\\") == 0){
                    break;
                }
                fflush(stdout);
            printf("Processo L: %s e' una parola palindroma\n", shm);
                fflush(stdout);

            SIGNAL(sem_id, 1);
        }
    }else if(argc == 3){
        FILE * file_id;

        if((file_id = fopen(file_name, "w")) == (FILE*)-1){
            perror("fopen: ");
            exit(1);
        }

        while(1){
            WAIT(sem_id, L);
                if(strcmp(shm, "\\logout\\") == 0){
                    fclose(file_id);
                    break;
                }
                fflush(stdout);
                fprintf(file_id,"Processo L: %s e' una parola palindroma\n", shm);
                fflush(stdout);

            SIGNAL(sem_id, 1);
        }
    }
}

int main(int argc, char *argv[]){
    int shm_id, sem_id;
    char * shm;
    if((shm_id = shmget(IPC_PRIVATE, MAX_LEN, IPC_CREAT | 0660)) == -1){
        perror("shmget: ");
        exit(1);
    }

    if((sem_id = semget(IPC_PRIVATE, 3, IPC_CREAT | 0660)) == -1){
        perror("semget: ");
        exit(1);
    }

    if(semctl(sem_id, 0, SETVAL, 0) ==-1){
        perror("setval sem0: ");
        exit(1);
    }
    if(semctl(sem_id, 1, SETVAL, 0) ==-1){
        perror("setval sem0: ");
        exit(1);
    }
    if(semctl(sem_id, 2, SETVAL, 0) ==-1){
        perror("setval sem0: ");
        exit(1);
    }

    if(argc == 2 || argc > 4){
        printf("Usage: %s [<input-file>]\n", argv[0]);
        exit(0);
    }
    
    if((shm = (char*)shmat(shm_id, NULL, 0)) == (char*)-1){
        perror("shmat: ");
        exit(1);
    }

    if(argc == 1){
        if(fork() == 0){
            R_process_routine(argc, NULL, sem_id, shm_id);
            exit(0);
        }
        if(fork() == 0){
            L_process_routine(argc, NULL, sem_id, shm_id);
            exit(0);
        }
    }else if(argc == 3){
        if(fork() == 0){
            R_process_routine(argc, argv[1], sem_id, shm_id);
            exit(0);
        }
        if(fork() == 0){
            L_process_routine(argc, argv[2], sem_id, shm_id);
            exit(0);
        }
    }

    while(1){
        WAIT(sem_id, 0);
        if(strcmp(shm, "\\logout\\") == 0){
            SIGNAL(sem_id, 2);
            break;
        }
            
        if(is_palindroma(shm)){
            //manda a P
            SIGNAL(sem_id, 2);
            //L sveglierà R
        }else
            SIGNAL(sem_id, 1);
            //svegliamo noi il processo R
    }

    wait(NULL);
    wait(NULL);

    //fine lavori
    if(semctl(sem_id, 0, IPC_RMID) ==-1){
        perror("SEM RMID: ");
        exit(1);
    }

    if(shmctl(shm_id, IPC_RMID, NULL) == -1){
        perror("shm rmid: ");
        exit(1);
    }


}