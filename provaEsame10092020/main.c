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

#define MAX_DIM_FILE 2048
#define MAX_DIM_STR 128

int is_palindroma(char * str){
    int len = (int)strlen(str);
    int pal_counter = 0;

    for(int i = 0; i < (int)len/2; i++){
        if(tolower(str[i]) == tolower(str[len - 1 - i]))
            pal_counter++;
    }

    if(pal_counter == (int)len/2)
        return 1;
    else return 0;

}

void R_process_routine(int pipe[2], char * file_name){
    char *map;
    int file_fd;
    struct stat sb;
    int start = 0;
    int stop = 0;
    char parola[MAX_DIM_STR];
    
    //mappatura del file
    if((file_fd = open(file_name, O_RDONLY)) == -1){
        perror("open: ");
        exit(1);
    }

    if((fstat(file_fd, &sb)) < -1){
        perror("stat: ");
        exit(1);
    }

    if((map = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, file_fd, 0)) == MAP_FAILED){
        perror("mmap: ");
        exit(1);
    }

    close(pipe[0]);

    if(write(pipe[1], map, MAX_DIM_FILE) == -1){
        perror("write: ");
        exit(1);
    }

    return;
}

void W_process_routine(int pipe[2]){
    close(pipe[1]);
    char parola[MAX_DIM_STR];

    while(1){
        if(read(pipe[0], parola, MAX_DIM_STR) == -1){
            perror("read W: ");
            exit(1);
        }

        if(strcmp(parola, "||END||") == 0){
            break;
        }

        printf("Processo W: %s e' palindroma\n", parola);
    }
        
}

int main(int argc, char *argv[]){
    int pipe_RP[2]; 
    int pipe_PW[2]; 
    char buff[MAX_DIM_FILE];
    char parola[MAX_DIM_STR];

    if(argc != 2){
        printf("Usage: %s <input file>\n", argv[0]);
        exit(0);
    }

    if((pipe(pipe_PW)) == -1){
        perror("pipe PW: ");
        exit(1);
    }

    if((pipe(pipe_RP)) == -1){
        perror("pipe RP: ");
        exit(1);
    }


    if(fork() == 0){
        R_process_routine(pipe_RP, argv[1]);
        exit(0);
    }

    if(fork() == 0){
        W_process_routine(pipe_PW);
        exit(0);
    }   
    close(pipe_RP[1]);
    close(pipe_PW[0]);

    if((read(pipe_RP[0], buff, MAX_DIM_FILE)) == -1){
            perror("read: ");
            exit(1);
    }

    sscanf(buff, "%[^\n]\n", parola);

    if(is_palindroma(parola)){
        if(write(pipe_PW[1], parola, sizeof(parola)) == -1){
            perror("write P: ");
            exit(1);
        }
    }

    for(int i = 0; i < strlen(buff); i++){
        if(buff[i] == '\n'){
            sscanf(&buff[i+1], "%[^\n]\n", parola);

            if(is_palindroma(parola)){
                if(write(pipe_PW[1], parola, sizeof(parola)) == -1){
                    perror("write P: ");
                    exit(1);
                }
            }
        }
    }

    if(write(pipe_PW[1], "||END||", 8) == -1){
        perror("write: ");
        exit(1);
    }

    
    wait(NULL);
    wait(NULL);
}
