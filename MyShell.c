#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64

#define GREEN  "\033[32m"
#define RESET  "\033[0m"

void print_banner(){
    printf(GREEN
        "\n"
        "    __  ___      _____ __         ____\n"
        "   /  |/  /_  __/ ___// /_  ___  / / /\n"
        "  / /|_/ / / / /\\__ \\/ __ \\/ _ \\/ / / \n"
        " / /  / / /_/ /___/ / / / /  __/ / /  \n"
        "/_/  /_/\\__, //____/_/ /_/\\___/_/_/   \n"
        "       /____/                         \n"
        "\n"
        "           by Martin Liarte\n\n"
        RESET
    );
}

int main(){
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    
    printf("\033[2J\033[H"); //clean screen
    print_banner();

    while(1){
        printf("MyShell> ");
        fflush(stdout);

        if(!fgets(input, sizeof(input), stdin)){
            break; //eof = ctrl + d
        }
        
        input[strcspn(input, "\n")] = 0;

        if(strlen(input) == 0){
            continue;
        } //ignore empty line(input length==0)

        int i = 0;
        char *token = strtok(input, " "); //tokenize by spaces
        while(token != NULL && i < MAX_ARGS -1){
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

       if(strcmp(args[0], "exit") == 0){
            break; 
        }

        pid_t pid = fork();

        if(pid == 0){
            //child
            execvp(args[0], args);
            perror("execvp error");
            exit(EXIT_FAILURE);
        }
        else if(pid > 0){
            wait(NULL); //father just waits the children execution to finish
        }

        else{
            perror("fork error");
        }

        //implement reading of environment variables
        // echo $PATH for example.






    }

}
