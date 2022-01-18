//ESAME del 02/07/21

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <string.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>

#define BUFFER 2048
#define SEMAFORO_PADRE 0
#define SEMAFORO_FIGLI 1

int WAIT(int sem_des, int num_semaforo) {
    struct sembuf operazioni[1] = {{num_semaforo, -1, 0}};
    return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, int num_semaforo) {
    struct sembuf operazioni[1] = {{num_semaforo, +1, 0}};
    return semop(sem_des, operazioni, 1);
}
typedef struct{
    long type;
    char oggettoAsta[50];
    int minOff;
    int maxOff;
    int currentOff;
    char idProcesso[3];
}asta;

void bidProcess(char name[3], int id_shm, int id_sem){
    WAIT(id_sem,SEMAFORO_FIGLI);
    asta* indirizzoMemoria;
    if((indirizzoMemoria=(asta*)shmat(id_shm,NULL,0))== (asta*)-1){
        perror("shmat");
        exit(1);
    }
    srand(time(0)+getpid()); //Se non aggiungessi il getPid il seed sarebbe uguale per tutti vista la velocit� di esecuzione del programma, quindi, come testato inizialmente, tutti numeri uguali per ogni asta.
    while(strcmp(indirizzoMemoria->oggettoAsta,"")!=0){
        int offerta;
        offerta=rand() % (indirizzoMemoria->maxOff+1);//Se l'offerta massima fosse 50, il rand darebbe massimo 49, quindi metto +1.
        indirizzoMemoria->currentOff=offerta;
        strcpy(indirizzoMemoria->idProcesso,name);
        printf("[%s] Invio offerta di %d EUR per asta con oggetto %s\n", name, offerta, indirizzoMemoria->oggettoAsta);
        SIGNAL(id_sem,SEMAFORO_PADRE);
        WAIT(id_sem,SEMAFORO_FIGLI);
    }
    exit(0);
}
int main(int argc, char* argv[]){
    if(argc<3){
        printf("N. argomenti inferiori per far partire il programma");
        exit(0);
    }
    FILE* file;
    char* pathName=argv[1];
    asta* indirizzoMemoria;
    key_t chiave=IPC_PRIVATE;
    int id_shm, id_coda, id_sem;
    file=fopen(pathName,"r");
    if((id_shm=shmget(chiave,BUFFER, IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("shmget");
        exit(-1);
    }
    if((indirizzoMemoria=(asta*)shmat(id_shm,NULL,0))== (asta*)-1){ 
        perror("shmat");
        exit(-1);
    }
    if((id_sem=semget(chiave,2,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("semget");
        exit(-1);
    }
    semctl(id_sem,SEMAFORO_PADRE,SETVAL,0);
    semctl(id_sem,SEMAFORO_FIGLI,SETVAL,0);
    int numBidders=atoi(argv[2]);
    for (int i=0;i<numBidders;i++){
        char name[3];
        snprintf(name,3,"B%d",i+1);
        if (fork()==0) bidProcess(name, id_shm, id_sem);
    }
    char rigaFile[1024];
    int contaAsta=0;
    int totaleRaccolto=0;
    int asteConSuccesso=0;
    while(fgets(rigaFile,BUFFER,file)!=NULL){
        int offerteValide=0;
        int offertaMaxRicevuta=-1;
        char migliorOfferente[3];
        contaAsta++;
        strcpy(indirizzoMemoria->oggettoAsta,strtok(rigaFile,","));
        indirizzoMemoria->minOff=atoi(strtok(NULL,","));
        indirizzoMemoria->maxOff=atoi(strtok(NULL,","));
        printf("[J] Lancio asta N.%d per l'oggetto %s con offerta minima di %d EUR e massima di %d EUR\n",contaAsta, indirizzoMemoria->oggettoAsta, indirizzoMemoria->minOff, indirizzoMemoria->maxOff);
        for (int i=0;i<numBidders;i++){
            SIGNAL(id_sem,SEMAFORO_FIGLI);
            WAIT(id_sem,SEMAFORO_PADRE);
            printf("[J] Ricevuta offerta da %s per l'asta N.%d\n",indirizzoMemoria->idProcesso,contaAsta);
            if (indirizzoMemoria->currentOff>=indirizzoMemoria->minOff) offerteValide++;
            if (indirizzoMemoria->currentOff>offertaMaxRicevuta){
                offertaMaxRicevuta=indirizzoMemoria->currentOff;
                strcpy(migliorOfferente,indirizzoMemoria->idProcesso);
            }
        }
        if (offerteValide>0){
            printf("L'asta N.%d per l'oggetto %s si e' conclusa con %d offerte valide su %d. Il vincitore e' %s\n\n",contaAsta,indirizzoMemoria->oggettoAsta,offerteValide,numBidders, migliorOfferente);
            totaleRaccolto+=offertaMaxRicevuta;
            asteConSuccesso++;
        }
        else printf("L'asta N.%d per l'oggetto %s si e' conclusa senza alcuna offerta valida, pertanto l'oggetto non risulta assegnato\n\n",contaAsta,indirizzoMemoria->oggettoAsta);
    }
    printf("\n[J] Sono state svolte %d aste di cui %d assegnate con successo e %d andate a vuoto. Il totale raccolto � di %d EUR.\n",contaAsta, asteConSuccesso, contaAsta-asteConSuccesso, totaleRaccolto);
    strcpy(indirizzoMemoria->oggettoAsta,"");
    for (int i=0;i<numBidders;i++) SIGNAL(id_sem,SEMAFORO_FIGLI);
    
    //Soluzione stupida ma funzionante per splittare in C
    /*char* prova=malloc(2048);
    strcpy(prova,"ciao,393,384");
    char* first_part=strtok(prova,",");
    char* second_part=strtok(NULL,",");
    char* third_part=strtok(NULL,",");
    printf("L'output �: %s %s %s",first_part,second_part,third_part);*/
}