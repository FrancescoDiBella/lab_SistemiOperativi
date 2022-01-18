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

int main(){
    char * str = "ciao\n\0" "ciao\0"; //c->0 i ->1 a ->2 o ->3 \n ->4 \0 ->5

    printf("%d", (int)strlen(str));
}