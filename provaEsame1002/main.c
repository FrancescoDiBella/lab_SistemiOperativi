#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define AL 0
#define MZ 1
#define SEM 2
#define OFFSET 13

int WAIT(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num , -1 , 0}};
    return semop(sem_id, op, 1);
}

int SIGNAL(int sem_id, int sem_num){
    struct sembuf op[1] = {{sem_num , 1 , 0}};
    return semop(sem_id, op, 1);
}

int is_AL_or_MZ(char c){
    char lowered = tolower(c);
    if(lowered >= 'a' && lowered <= 'l'){
        return AL;
    }else if(lowered >= 'm' && lowered <= 'z'){
        return MZ;
    }else return -1;
}

void AL_MZ_routine(int CHAR_id, int STATS_id, int sem_id, int type_process){
    char * CHAR;
    int * STATS;
    char lettera;

    if((CHAR = (char *)shmat(CHAR_id, NULL, 0)) == (char *)-1){
        perror("shmat CHAR: ");
        exit(1);
    }

    if((STATS = (int *)shmat(STATS_id, NULL, 0)) == (int *)-1){
        perror("shmat CHAR: ");
        exit(1);
    }

    while(1){
        WAIT(sem_id, type_process);
        lettera = *CHAR;

        if(lettera != '/'){
            STATS[tolower(lettera) - 'a'] += 1;
        }else{
            SIGNAL(sem_id, SEM);
            return;
        }

        SIGNAL(sem_id, SEM);
    }

}

int main(int argc, char * argv[]){
    
    int sem_id, CHAR_id, STATS_id;
    int file_fd;
    char *map ;
    char *CHAR;
    int *STATS;
    struct stat sb;

    if(argc != 2){
        printf("Usage: %s <file.txt>\n", argv[0]);
        exit(0);
    }

    if((sem_id = semget(IPC_PRIVATE, 3, IPC_CREAT | 0660)) ==  -1){
        perror("semget: ");
        exit(1);
    }

    if((semctl(sem_id, AL, SETVAL, 0)) == -1){
        perror("smctl AL: ");
        exit(1);
    }

    if((semctl(sem_id, MZ, SETVAL, 0)) == -1){
        perror("smctl AL: ");
        exit(1);
    }

    if((semctl(sem_id, SEM, SETVAL, 0)) == -1){
        perror("smctl AL: ");
        exit(1);
    }

    if((CHAR_id = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | 0660)) == -1){
        perror("CHAR get: ");
        exit(1);
    }

    if((STATS_id = shmget(IPC_PRIVATE, sizeof(long)*26, IPC_CREAT | 0660)) == -1){
        perror("CHAR get: ");
        exit(1);
    }

    if((CHAR = (char *)shmat(CHAR_id, NULL, 0)) == (char *)-1){
        perror("shmat CHAR: ");
        exit(1);
    }

    if((STATS = (int *)shmat(STATS_id, NULL, 0)) == (int *)-1){
        perror("shmat CHAR: ");
        exit(1);
    }

    for(int i = 0; i < 26; i++){
        STATS[i] = 0;
    }

    if((file_fd = open(argv[1], O_RDONLY)) == -1){
        perror("open: ");
        exit(1);
    }

    if(fstat(file_fd, &sb) < -1 ){
        perror("fstat: ");
        exit(1);
    }

    if((map = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file_fd, 0)) == MAP_FAILED){
        perror("mmap: ");
        exit(1);
    }

    if(fork() == 0){
        AL_MZ_routine(CHAR_id, STATS_id, sem_id, AL);
        exit(0);
    }

    if(fork() == 0){
        AL_MZ_routine(CHAR_id, STATS_id, sem_id, MZ);
        exit(0);
    }

    for(int i = 0; i < sb.st_size; i++){
        if(is_AL_or_MZ(map[i]) == AL){
            
            *CHAR = map[i];
            SIGNAL(sem_id, AL);
            WAIT(sem_id, SEM);

        }else if(is_AL_or_MZ(map[i]) == MZ){

            *CHAR = map[i];
            SIGNAL(sem_id, MZ);
            WAIT(sem_id, SEM);
            
        }
    }


    printf("Statistiche: \n");
    
    for(int i = 0; i < 26; i++){
        printf("[%c]:   n. %d occorrenze su n. %ld di caratteri del file %s\n", i + 'A', STATS[i], sb.st_size, argv[1]);
    }

            //  fine lavori
            *CHAR = '/';
            SIGNAL(sem_id, AL);
            WAIT(sem_id, SEM);

            SIGNAL(sem_id, MZ);
            WAIT(sem_id, SEM);

    if(semctl(sem_id, 0, IPC_RMID) == -1){
        perror("semctl: ");
        exit(1);
    }

    if(shmctl(CHAR_id, IPC_RMID, NULL) == -1){
        perror("semctl: ");
        exit(1);
    }

    if(shmctl(STATS_id, IPC_RMID, NULL) == -1){
        perror("semctl: ");
        exit(1);
    }

    if(munmap(map, sb.st_size) == -1){
        perror("munmap: ");
        exit(1);
    }

    return 0;
}