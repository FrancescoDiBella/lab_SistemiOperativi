#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

int main(){

    char str[] = "ciao";
    int num[] = {1,2,3};

    char dest[4];

    memcpy(dest, num, 3);
    dest[3] = '\0';

    
    printf("%s", dest);

    return 0;
}