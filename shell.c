#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

int main(){
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    while(1){
        printf("MyShell> ");
        fflush(stdout);

        if(!fgets(input, sizeof(input), stdin)){
            break; //eof = ctrl + d
        }
    }

}