#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

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

void expand_environment_variables(char **args){
    for(int i = 0; args[i] != NULL; i++){
        if(args[i][0] == '$'){
            char *env_var = getenv(args[i] + 1); //skip the '$' character
            if(env_var){
                args[i] = env_var; //replace with environment variable value
            }
        }
    }
}

int handle_builtin(char **args){
    if (args[0] == NULL) {
    return 1;
    }

    if (strcmp(args[0], "cd") == 0) {
        char cwd[1024];
        char *dir = NULL;

        // save current directory before changing 
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd");
            return 1;
        }

        // determine target directory based on arguments
        if (args[1] != NULL) {
            // if the argument is "-", change to OLDPWD
            if (strcmp(args[1], "-") == 0) {
                dir = getenv("OLDPWD");  // get OLDPWD
                if (dir == NULL) {
                    fprintf(stderr, "cd: OLDPWD is not set\n");
                    return 1;
                }
                printf("%s\n", dir); // Bash prints the new directory when using "cd -"
            } else {
                dir = args[1]; // use the provided argument as the target directory
            }
        } else {
            // if no argument is provided, change to HOME
            dir = getenv("HOME");
            if (dir == NULL) dir = "/"; // fallback to root if HOME is not set
        }

        // change directory and handle errors
        if (chdir(dir) != 0) {
            perror("cd error");
            return 1;
        }

        // update OLDPWD environment variable
        setenv("OLDPWD", cwd, 1);

        return 1;
    }

    if(strcmp(args[0], "pwd") == 0){
        char cwd[1024];
        if(getcwd(cwd, sizeof(cwd)) != NULL){
            printf("%s\n", cwd);
        }
        else{
            perror("getcwd error");
        }
        return 1;
    }

    if(strcmp(args[0], "clear") == 0){
        printf("\033[2J\033[H"); // clear screen
        return 1;
    }

    if(strcmp(args[0], "exit") == 0){
        exit(0);
    }

    return 0;
}

int main(){
    char input[MAX_INPUT];
    char *args[MAX_ARGS];
    
    printf("\033[2J\033[H"); //clear screen
    print_banner();

    while(1){
        char dir_name[1024];

        if (getcwd(dir_name, sizeof(dir_name)) == NULL) {
        strcpy(dir_name, "?");   // fallback if getcwd fails
        }

        char *base = strrchr(dir_name, '/');
        if (base != NULL) {
            base = base + 1;   // move past the '/' character
        } 
        else {
            base = dir_name;  // if no '/' found, use the whole string
        }

        printf("MyShell(%s)> ", base);

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

        expand_environment_variables(args);

        if(handle_builtin(args)) {
        continue;
        }

        pid_t pid = fork();

        if(pid == 0){
            //child
            execvp(args[0], args);
            perror("execvp error");
            exit(EXIT_FAILURE);
        }
        else if(pid > 0){
            waitpid(pid, NULL, 0); //wait for child process to finish
        }
        else{
            perror("fork error");
        }
    }
}