
    /*
    int file_fd;
    char *map ;
    struct stat sb;

    if((file_fd = open(argv[1], O_RDONLY)) == -1){
        perror("open: ");
        exit(1);
    }

    printf("Tutto ok! file_fd: %d", file_fd);

    if(fstat(file_fd, &sb) < 0){
        perror("fstat: ");
        exit(1);
    }

    if((map = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED,
        file_fd, 0)) == MAP_FAILED)
    {       
        perror("map: ");
        exit(1);
    }

    for(int i = 0; i < strlen(map); i++){
        printf("%c", map[i]);
    }


    close(file_fd);
    */
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

int main(){
    char r[120];
    char s[120];
    int a = 0;
    int b = 0;
    char c = 'b';
    strcpy(r, "Cotta di Mithril,100,500,\n");

    sscanf(r, "%[^,],%d,%d", s, &a, &b);
    printf("%s, %d, %d", s, a, b);

    printf("%d", c - 'a');


    return 0;
}
