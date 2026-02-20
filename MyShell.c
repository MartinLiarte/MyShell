#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define GREEN  "\033[32m"
#define RESET  "\033[0m"

/* =========================
   VISUAL UTILITIES
   ========================= */

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

void print_prompt() {
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }

    char *base = strrchr(cwd, '/');
    base = (base != NULL) ? base + 1 : cwd;

    printf("MyShell(%s)> ", base);
    fflush(stdout);
}

/* =====================================================
   SIGNAL HANDLING
   ===================================================== */

void handle_sigint(int sig) {
    (void)sig;
    printf("\n");
}

/* =====================================================
   INPUT HANDLING
   ===================================================== */

/*
 * Reads a line from standard input.
 * Returns:
 *   0  -> EOF detected (Ctrl+D)
 *  -1  -> Empty line
 *   1  -> Valid input
 */
int read_input(char *buffer) {
    if (!fgets(buffer, MAX_INPUT, stdin)) {
        return 0;  // EOF
    }

    buffer[strcspn(buffer, "\n")] = 0;  // Remove trailing newline

    if (strlen(buffer) == 0) {
        return -1;  // Empty line
    }

    return 1;
}

/* =====================================================
   PARSING
   ===================================================== */

/*
 * Splits the input string into arguments separated by
 * spaces or tabs.
 */
void parse_input(char *input, char **args) {
    int i = 0;
    char *token = strtok(input, " \t");

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t");
    }

    args[i] = NULL;
}

/* =====================================================
   ENVIRONMENT VARIABLE EXPANSION
   ===================================================== */

/*
 * Replaces arguments that begin with '$'
 * with their corresponding environment value.
 */
void expand_environment_variables(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (args[i][0] == '$') {
            char *env_value = getenv(args[i] + 1);
            if (env_value) {
                args[i] = env_value;
            }
        }
    }
}

/* =========================
   BUILTINS
   ========================= */

int builtin_cd(char **args) {
    char cwd[1024];
    char *dir = NULL;

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    if (args[1] != NULL) {
        if (strcmp(args[1], "-") == 0) {
            dir = getenv("OLDPWD");
            if (!dir) {
                fprintf(stderr, "cd: OLDPWD not set\n");
                return 1;
            }
            printf("%s\n", dir);
        } else {
            dir = args[1];
        }
    } else {
        dir = getenv("HOME");
        if (!dir) dir = "/";
    }

    if (chdir(dir) != 0) {
        perror("cd");
        return 1;
    }

    setenv("OLDPWD", cwd, 1);
    return 1;
}

int builtin_pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
    }
    return 1;
}

int handle_builtin(char **args) {
    if (args[0] == NULL)
        return 1;

    if (strcmp(args[0], "cd") == 0)
        return builtin_cd(args);

    if (strcmp(args[0], "pwd") == 0)
        return builtin_pwd();

    if (strcmp(args[0], "clear") == 0) {
        printf("\033[2J\033[H");
        return 1;
    }

    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }

    return 0;  // not builtin
}

/* =====================================================
   COMMAND EXECUTION
   ===================================================== */

/*
 * Executes an external command using fork + execvp.
 */
void execute_command(char **args) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0) {
        // Parent process
        waitpid(pid, NULL, 0);
    }
    else {
        perror("fork");
    }
}

/* =========================
   MAIN
   ========================= */

int main() {
    char input[MAX_INPUT];
    char *args[MAX_ARGS];

    signal(SIGINT, handle_sigint);

    printf("\033[2J\033[H");
    print_banner();

    while (1) {
        print_prompt();

        int status = read_input(input);

        if (status == 0) break;      // EOF (Ctrl+D)
        if (status == -1) continue;  // Empty line

        parse_input(input, args);
        expand_environment_variables(args);

        if (!handle_builtin(args)) {
            execute_command(args);
        }
    }

    return 0;
}