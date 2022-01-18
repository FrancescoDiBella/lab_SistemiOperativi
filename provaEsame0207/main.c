#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <malloc.h>

#define MAX_DESCR_LEN 128
#define MAX_ASTA_LEN 256
#define J 0
#define Bn 1

void utility_ipc(int shm_id, int sem_id){
    if(semctl(sem_id, 0, IPC_RMID) == -1){
        perror("semctl RMID: ");
        exit(1);
    }
    if(shmctl(shm_id, IPC_RMID, 0) == -1){
        perror("shmctl RMID: ");
        exit(1);
    }
}

typedef struct {
    char description[MAX_DESCR_LEN];
    int min_offer;
    int max_offer;
    int current_offer;
    int process_id;
}auction;

typedef struct{
    int offerta;
    int time_stamp;
} resoconto;


int WAIT(int sem_id, int sem_num, int n){
    struct sembuf op[1] = {{sem_num, -n, 0}};
    return semop(sem_id, op, 1);
}

int SIGNAL(int sem_id, int sem_num, int n){
    struct sembuf op[1] = {{sem_num, n, 0}};
    return semop(sem_id, op, 1);
}

void judge_process_routine(char * file_name, int shm_id, int sem_id, int num_bidd){
    FILE * file; 
    auction * auct;
    char asta[MAX_ASTA_LEN];
    resoconto offerte_correnti[num_bidd];
    int scale = 0;
    int limit = num_bidd;
    //fopen file
    if((file = fopen(file_name, "r")) == (FILE*) -1){
        perror("fopen: ");
        utility_ipc(shm_id, sem_id);
        exit(1);
    }

    if((auct = (auction*)shmat(shm_id, NULL, 0)) == (auction*)-1){
        perror("shmat: ");
        utility_ipc(shm_id, sem_id);
        exit(1);
    }


    while(fgets(asta, MAX_ASTA_LEN, file)){
        int leader = -1;
        int first_leader = 0;
        int leader_exq = -1;

        
        sscanf(asta, "%[^,],%d,%d", auct->description, &auct->min_offer, &auct->max_offer);        
        for(int i = 0; i < num_bidd; i++){
            //aspetta che le offerte arrivino
            SIGNAL(sem_id, J, 1 + (scale));
            WAIT(sem_id, Bn, 1);
                offerte_correnti[auct->process_id].offerta = auct->current_offer;
                offerte_correnti[auct->process_id].time_stamp = i;

            printf("Offerta da Bidder-%d per %s ammonta a %d$, offerta minima %d$ \n", auct->process_id, auct->description, offerte_correnti[auct->process_id].offerta, auct->min_offer );
            
        }

        for(int i = 0; i <num_bidd; i++){
            if(offerte_correnti[i].offerta >= auct->min_offer && (first_leader == 0)){
                leader = i;
                first_leader = 1;
            }else if(leader > -1 && offerte_correnti[i].offerta > offerte_correnti[leader].offerta){
                leader = i;

            }else if(leader > -1 && offerte_correnti[i].offerta == offerte_correnti[leader].offerta){
                leader_exq = i;
            }   
        }
        
            fflush(stdout);

        if(leader == -1){
            printf("Nessuna offerta ha superato il minimo.\n");
        }else if (leader_exq ==-1){
            printf("L'offerta di %d$ del Bidder %d ha superato le altre e si aggiudica l'asta.\n", offerte_correnti[leader].offerta, leader+1);
        }else{
            if(offerte_correnti[leader].offerta == offerte_correnti[leader_exq].offerta){
                if(offerte_correnti[leader_exq].time_stamp < offerte_correnti[leader].time_stamp){
                    printf("L'offerta di %d$ del Bidder %d era in exequo,ha superato le altre e si aggiudica l'asta.\n", offerte_correnti[leader_exq].offerta, leader_exq+1);
                }else{
                    printf("L'offerta di %d$ del Bidder %d era in exequo,ha superato le altre e si aggiudica l'asta.\n", offerte_correnti[leader].offerta, leader+1);
                }
            }else if(offerte_correnti[leader].offerta > offerte_correnti[leader_exq].offerta){
                printf("L'offerta di %d$ del Bidder %d era in exequo,ha superato le altre e si aggiudica l'asta.\n", offerte_correnti[leader].offerta, leader+1);
            }else{
                printf("L'offerta di %d$ del Bidder %d era in exequo,ha superato le altre e si aggiudica l'asta.\n", offerte_correnti[leader_exq].offerta, leader_exq+1);
            }
        }

            fflush(stdout);

            scale += 1;
    }

    strcpy(auct->description, "END");

    for(int i = 0; i < limit; i++){

        SIGNAL(sem_id, J,  1 + (scale));

    }


}

void bidder_process_routine(int pid, int shm_id, int sem_id, int num_bidd){
    auction* auct;
    int scale = 0;
    //memat
    if((auct = (auction*) shmat(shm_id, NULL, 0)) == (auction*)-1){
        perror("shmat BIDD: ");
        exit(1);
    }

    while(1){
        time_t t;
        srand((int)time(&t) % getpid());
        WAIT(sem_id, J, 1 + scale);
            if (strcmp(auct->description, "END") == 0)
            {   
                return;
            }
            
            int off = auct->max_offer;
            auct->current_offer= rand() % (off+1);
            auct->process_id = pid-1;
            fflush(stdout);
            printf("Processo BIDD %d con id %d---> %s la mia offerta %d$\n", pid, pid+scale, auct->description, auct->current_offer );
            fflush(stdout);

        SIGNAL(sem_id, Bn, 1);
        scale += 1;
    }
}

int main(int argc, char* argv[]){
    int sem_id, shm_id;

    if(argc != 3){
        printf("Usage: %s <auction-file> <num-bidders> \n", argv[0]);
        exit(0);
    }

    if((sem_id = semget(IPC_PRIVATE, 2, IPC_CREAT | 0660)) == -1){
        perror("semget: ");
        exit(1);
    }

    if(semctl(sem_id, J, SETVAL, 0)){
        perror("semctl: ");
        exit(1);
    }

    if(semctl(sem_id, Bn, SETVAL, 0)){
        perror("semctl: ");
        exit(1);
    }

    if((shm_id = shmget(IPC_PRIVATE, sizeof(auction), IPC_CREAT | 0660)) == -1){
        perror("shmget: ");
        exit(1);
    }

    if(fork() == 0){
        judge_process_routine(argv[1], shm_id, sem_id, atoi(argv[2]));
        exit(0);
    }

    for(int i = 0; i < atoi(argv[2]); i++){
        if(fork() == 0){
            bidder_process_routine(i+1,shm_id, sem_id, atoi(argv[2]));
            exit(0);
        }
    }


    for(int i = 0; i < 1 + atoi(argv[2]); i++){
        wait(NULL);
    }


    if((shmctl(shm_id, IPC_RMID, NULL)) == -1){
        perror("shmctl RMID: ");
        exit(1);
    }
    if((semctl(sem_id, 0, IPC_RMID)) == -1){
        perror("semctl RMID: ");
        exit(1);
    }
    return 0;
}

