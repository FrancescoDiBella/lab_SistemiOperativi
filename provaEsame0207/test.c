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

    strcpy(r, "Cotta di Mithril,100,500,\n");

    sscanf(r, "%[^,],%d,%d", s, &a, &b);
    printf("%s, %d, %d", s, a, b);

    return 0;
}