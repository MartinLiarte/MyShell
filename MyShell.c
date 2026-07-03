/*
 * MyShell - Unix Shell
 * by Martin Liarte
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <readline/readline.h>
#include <readline/history.h>

/* ─── Constants ─────────────────────────────────────────────────────────── */

#define MAX_ARGS      64
#define MAX_PIPELINE  16

/* ─── ANSI colours ───────────────────────────────────────────────────────── */

#define GREEN   "\033[32m"
#define CYAN    "\033[36m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

/* ═══════════════════════════════════════════════════════════════════════════
   FORWARD DECLARATIONS
   ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char  *argv[MAX_ARGS];
    char  *input_file;
    char  *output_file;
    int    append;
} Cmd;

static void free_pipeline(Cmd pipeline[], int n);

/* ═══════════════════════════════════════════════════════════════════════════
   VISUAL UTILITIES
   ═══════════════════════════════════════════════════════════════════════════ */

static void print_banner(void) {
    printf(GREEN BOLD
        "\n"
        "    __  ___      _____ __         ____\n"
        "   /  |/  /_  __/ ___// /_  ___  / / /\n"
        "  / /|_/ / / / /\\__ \\/ __ \\/ _ \\/ / / \n"
        " / /  / / /_/ /___/ / / / /  __/ / /  \n"
        "/_/  /_/\\__, //____/_/ /_/\\___/_/_/   \n"
        "       /____/                          \n"
        RESET "\n"
        "           by Martin Liarte\n\n"
    );
}

static char *build_prompt(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        strcpy(cwd, "?");

    char *base = strrchr(cwd, '/');
    base = (base && base[1]) ? base + 1 : cwd;

    /* Non-printing sequences must be wrapped in \001..\002 so readline
       excludes them when computing the prompt width; otherwise line
       editing and history navigation misplace the cursor. */
    size_t prompt_size = (size_t)PATH_MAX + 64;
    char  *prompt = malloc(prompt_size);
    if (!prompt) return strdup("$ ");
    snprintf(prompt, prompt_size,
        "\001" CYAN BOLD "\002" "MyShell" "\001" RESET "\002"
        "(" "\001" YELLOW "\002" "%s" "\001" RESET "\002" ")"
        "\001" BOLD "\002" ">" "\001" RESET "\002" " ",
        base);
    return prompt;
}

/* ═══════════════════════════════════════════════════════════════════════════
   SIGNAL HANDLING
   ═══════════════════════════════════════════════════════════════════════════ */

static volatile sig_atomic_t sigint_received = 0;

static void handle_sigchld(int sig) {
    (void)sig;
    int saved = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved;
}

static void handle_sigint(int sig) {
    (void)sig;
    sigint_received = 1;
}

static void process_sigint(void) {
    sigint_received = 0;
    printf("\n");
    rl_on_new_line();
#ifdef HAVE_RL_REPLACE_LINE
    rl_replace_line("", 0);
#else
    if (rl_line_buffer)
        rl_line_buffer[0] = '\0';
    rl_point = rl_end = 0;
#endif
    rl_redisplay();
}

/* ═══════════════════════════════════════════════════════════════════════════
   TILDE & VARIABLE EXPANSION
   ═══════════════════════════════════════════════════════════════════════════ */

static int buf_append(char **buf, size_t *len, size_t *cap, char c) {
    if (*len + 1 >= *cap) {
        size_t new_cap = (*cap) * 2;
        char *tmp = realloc(*buf, new_cap);
        if (!tmp) return -1;
        *buf  = tmp;
        *cap  = new_cap;
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len]     = '\0';
    return 0;
}

static const char *expand_var_inline(const char *s,
                                     char **buf, size_t *len, size_t *cap)
{
    const char *start = s;
    while (*s && (isalnum((unsigned char)*s) || *s == '_'))
        s++;

    if (s > start) {
        char varname[256];
        size_t vlen = (size_t)(s - start);
        if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
        memcpy(varname, start, vlen);
        varname[vlen] = '\0';

        const char *val = getenv(varname);
        if (val) {
            for (; *val; val++) {
                if (*len + 1 >= *cap) {
                    size_t new_cap = (*cap) * 2;
                    char *tmp = realloc(*buf, new_cap);
                    if (!tmp) break;
                    *buf = tmp;
                    *cap = new_cap;
                }
                (*buf)[(*len)++] = *val;
                (*buf)[*len] = '\0';
            }
        }
    } else {
        buf_append(buf, len, cap, '$');
    }
    return s;
}

static char *expand_token(const char *tok) {
    if (tok[0] == '~') {
        const char *home = getenv("HOME");
        if (!home) home = "/";
        if (tok[1] == '\0') {
            return strdup(home);
        } else if (tok[1] == '/') {
            size_t n = strlen(home) + strlen(tok + 1) + 2;
            char *out = malloc(n);
            if (!out) return strdup("");
            snprintf(out, n, "%s%s", home, tok + 1);
            return out;
        }
    }
    return strdup(tok);
}

/* ═══════════════════════════════════════════════════════════════════════════
   QUOTE-AWARE TOKENISER
   ═══════════════════════════════════════════════════════════════════════════ */

static int flush_token(char **buf, size_t *len, int *in_token, int *tilde_ok,
                       char *out[], int *ntok, int max_tok)
{
    if (!*in_token) return 0;
    if (*ntok < max_tok) {
        /* Tilde expansion only applies when the word starts with an
           unquoted '~' (same rule as bash). */
        out[(*ntok)++] = *tilde_ok ? expand_token(*buf) : strdup(*buf);
    } else {
        fprintf(stderr, YELLOW
            "myshell: warning: argument list truncated (limit is %d)\n"
            RESET, max_tok);
    }
    *len = 0;
    (*buf)[0] = '\0';
    *in_token = 0;
    *tilde_ok = 0;
    return 1;
}

static int tokenise(const char *s, char *out[], int max_tok) {
    int    ntok      = 0;
    size_t cap = 64;
    char  *buf = malloc(cap);
    if (!buf) return 0;
    size_t len = 0;
    buf[0] = '\0';

    int in_single = 0, in_double = 0, in_token = 0, tilde_ok = 0;

    while (*s) {
        char c = *s;

        if (in_single) {
            if (c == '\'') { in_single = 0; }
            else           { buf_append(&buf, &len, &cap, c); in_token = 1; }
            s++;
            continue;
        }

        if (in_double) {
            if (c == '"')  { in_double = 0; s++; continue; }
            if (c == '$')  {
                s = expand_var_inline(s + 1, &buf, &len, &cap);
                in_token = 1;
                continue;
            }
            buf_append(&buf, &len, &cap, c);
            in_token = 1;
            s++;
            continue;
        }

        if (c == '\'') { in_single = 1; in_token = 1; s++; continue; }
        if (c == '"')  { in_double = 1; in_token = 1; s++; continue; }

        if (c == ' ' || c == '\t') {
            flush_token(&buf, &len, &in_token, &tilde_ok, out, &ntok, max_tok);
            s++;
            continue;
        }

        if (c == '$') {
            s = expand_var_inline(s + 1, &buf, &len, &cap);
            in_token = 1;
            continue;
        }

        if (c == '>' || c == '<') {
            flush_token(&buf, &len, &in_token, &tilde_ok, out, &ntok, max_tok);
            const char *op = (c == '>' && *(s + 1) == '>') ? ">>"
                           : (c == '>'                     ? ">"
                                                           : "<");
            /* FIX #26: warn when operator token cannot be stored */
            if (ntok < max_tok) {
                out[ntok++] = strdup(op);
            } else {
                fprintf(stderr, YELLOW
                    "myshell: warning: argument list truncated (limit is %d)\n"
                    RESET, max_tok);
            }
            s += strlen(op);
            continue;
        }

        if (!in_token) tilde_ok = 1;
        buf_append(&buf, &len, &cap, c);
        in_token = 1;
        s++;
    }

    if (in_single || in_double) {
        fprintf(stderr, RED "myshell: syntax error: unterminated %s quote\n"
                RESET, in_single ? "single" : "double");
        free(buf);
        for (int i = 0; i < ntok; i++) free(out[i]);
        return -1;
    }

    flush_token(&buf, &len, &in_token, &tilde_ok, out, &ntok, max_tok);
    free(buf);
    return ntok;
}

static int split_on_pipe(char *line, char *segs[], int max_seg) {
    int   nseg      = 0;
    int   in_sq     = 0, in_dq = 0;
    char *seg_start = line;

    for (char *p = line; *p; p++) {
        if (!in_dq && *p == '\'') { in_sq = !in_sq; continue; }
        if (!in_sq && *p == '"')  { in_dq = !in_dq; continue; }
        if (!in_sq && !in_dq && *p == '|') {
            if (nseg < max_seg - 1) {
                *p = '\0';
                segs[nseg++] = seg_start;
                seg_start = p + 1;
            } else {
                fprintf(stderr, RED
                    "myshell: pipeline too long (limit is %d stages)\n"
                    RESET, max_seg);
                return -1;
            }
        }
    }
    segs[nseg++] = seg_start;
    return nseg;
}

/* ═══════════════════════════════════════════════════════════════════════════
   PARSING
   ═══════════════════════════════════════════════════════════════════════════ */

static int parse_pipeline(char *line, Cmd pipeline[MAX_PIPELINE]) {
    int   stage = 0;
    char *segs[MAX_PIPELINE];

    memset(pipeline, 0, sizeof(Cmd) * MAX_PIPELINE);

    int nseg = split_on_pipe(line, segs, MAX_PIPELINE);
    if (nseg < 0)
        return 0;

    for (int s = 0; s < nseg; s++) {
        Cmd  *cmd  = &pipeline[stage];
        char *toks[MAX_ARGS];
        int   ntok = tokenise(segs[s], toks, MAX_ARGS);

        if (ntok < 0) {
            free_pipeline(pipeline, stage);
            memset(pipeline, 0, sizeof(Cmd) * MAX_PIPELINE);
            return 0;
        }

        if (ntok == 0 && nseg > 1) {
            fprintf(stderr, RED
                "myshell: syntax error near '|': missing command\n" RESET);
            free_pipeline(pipeline, stage);
            memset(pipeline, 0, sizeof(Cmd) * MAX_PIPELINE);
            return 0;
        }

        stage++;
        int argc    = 0;
        int syn_err = 0;
        int argv_full_warned = 0;
        int i;

        for (i = 0; i < ntok; i++) {
            char *t = toks[i];

            if (strcmp(t, "<") == 0) {
                free(t);
                if (++i < ntok) {
                    free(cmd->input_file);
                    cmd->input_file = toks[i];
                } else {
                    fprintf(stderr, RED
                        "myshell: syntax error: expected filename after '<'\n"
                        RESET);
                    syn_err = 1;
                }
            } else if (strcmp(t, ">>") == 0) {
                free(t);
                if (++i < ntok) {
                    free(cmd->output_file);
                    cmd->output_file = toks[i];
                    cmd->append = 1;
                } else {
                    fprintf(stderr, RED
                        "myshell: syntax error: expected filename after '>>'\n"
                        RESET);
                    syn_err = 1;
                }
            } else if (strcmp(t, ">") == 0) {
                free(t);
                if (++i < ntok) {
                    free(cmd->output_file);
                    cmd->output_file = toks[i];
                    cmd->append = 0;
                } else {
                    fprintf(stderr, RED
                        "myshell: syntax error: expected filename after '>'\n"
                        RESET);
                    syn_err = 1;
                }
            } else {
                if (argc < MAX_ARGS - 1) {
                    cmd->argv[argc++] = t;
                } else {
                    if (!argv_full_warned) {
                        fprintf(stderr, YELLOW
                            "myshell: warning: argument list truncated "
                            "(limit is %d)\n" RESET, MAX_ARGS - 1);
                        argv_full_warned = 1;
                    }
                    free(t);
                }
            }
        }

        cmd->argv[argc] = NULL;

        if (syn_err) {
            free_pipeline(pipeline, stage);
            memset(pipeline, 0, sizeof(Cmd) * MAX_PIPELINE);
            return 0;
        }
    }

    return stage;
}

static void free_pipeline(Cmd pipeline[], int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; pipeline[i].argv[j]; j++)
            free(pipeline[i].argv[j]);
        free(pipeline[i].input_file);
        free(pipeline[i].output_file);
        memset(&pipeline[i], 0, sizeof(Cmd));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   BUILTINS
   ═══════════════════════════════════════════════════════════════════════════ */

static int builtin_cd(char **args) {
    char cwd[PATH_MAX];
    char *dir = NULL;

    if (getcwd(cwd, sizeof(cwd)) == NULL) { perror("getcwd"); return 1; }

    if (args[1] != NULL) {
        if (strcmp(args[1], "-") == 0) {
            dir = getenv("OLDPWD");
            if (!dir) { fprintf(stderr, RED "cd: OLDPWD not set\n" RESET); return 1; }
            printf("%s\n", dir);
        } else {
            dir = args[1];
        }
    } else {
        dir = getenv("HOME");
        if (!dir) dir = "/";
    }

    if (chdir(dir) != 0) { perror("cd"); return 1; }
    setenv("OLDPWD", cwd, 1);
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        setenv("PWD", cwd, 1);
    return 1;
}

static int builtin_pwd(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("%s\n", cwd);
    else
        perror("pwd");
    return 1;
}

static int builtin_echo(char **args) {
    int newline = 1;
    int start   = 1;

    if (args[1] && strcmp(args[1], "-n") == 0) {
        newline = 0;
        start   = 2;
    }
    for (int i = start; args[i]; i++) {
        if (i > start) putchar(' ');
        fputs(args[i], stdout);
    }
    if (newline) putchar('\n');
    return 1;
}

static int builtin_export(char **args) {
    if (!args[1]) {
        fprintf(stderr, RED "export: usage: export VAR=value [VAR=value ...]\n"
                RESET);
        return 1;
    }
    for (int i = 1; args[i]; i++) {
        char *eq = strchr(args[i], '=');
        if (!eq) continue;
        *eq = '\0';
        setenv(args[i], eq + 1, 1);
        *eq = '=';
    }
    return 1;
}

static int builtin_env(void) {
    extern char **environ;
    for (char **e = environ; *e; e++)
        printf("%s\n", *e);
    return 1;
}

static int builtin_history(void) {
    int total = history_base + history_length - 1;
    for (int i = history_base; i <= total; i++) {
        HIST_ENTRY *entry = history_get(i);
        if (entry)
            printf(CYAN "%4d" RESET "  %s\n", i, entry->line);
    }
    return 1;
}

static int handle_builtin(char **args) {
    if (!args || !args[0]) return 1;

    if (strcmp(args[0], "cd")      == 0) return builtin_cd(args);
    if (strcmp(args[0], "pwd")     == 0) return builtin_pwd();
    if (strcmp(args[0], "echo")    == 0) return builtin_echo(args);
    if (strcmp(args[0], "export")  == 0) return builtin_export(args);
    if (strcmp(args[0], "env")     == 0) return builtin_env();
    if (strcmp(args[0], "history") == 0) return builtin_history();
    if (strcmp(args[0], "clear")   == 0) { printf("\033[2J\033[H"); return 1; }
    if (strcmp(args[0], "exit")    == 0) {
        int code = args[1] ? atoi(args[1]) : 0;
        printf(YELLOW "Bye!\n" RESET);
        exit(code);
    }
    return 0;
}

static int is_builtin_name(const char *name) {
    if (!name) return 0;
    return (strcmp(name, "cd")      == 0 ||
            strcmp(name, "pwd")     == 0 ||
            strcmp(name, "echo")    == 0 ||
            strcmp(name, "export")  == 0 ||
            strcmp(name, "env")     == 0 ||
            strcmp(name, "history") == 0 ||
            strcmp(name, "clear")   == 0 ||
            strcmp(name, "exit")    == 0);
}

static int is_parent_builtin(const char *name) {
    if (!name) return 0;
    return (strcmp(name, "cd")     == 0 ||
            strcmp(name, "export") == 0 ||
            strcmp(name, "exit")   == 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
   PIPELINE EXECUTION
   ═══════════════════════════════════════════════════════════════════════════ */

static void apply_redirections(Cmd *cmd) {
    if (cmd->input_file) {
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0) { perror(cmd->input_file); exit(EXIT_FAILURE); }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (cmd->output_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd    = open(cmd->output_file, flags, 0644);
        if (fd < 0) { perror(cmd->output_file); exit(EXIT_FAILURE); }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

static int has_redirections(const Cmd *cmd) {
    return (cmd->input_file != NULL || cmd->output_file != NULL);
}

static void execute_pipeline(Cmd pipeline[], int n, int background) {

    if (n == 1) {
        char *name     = pipeline[0].argv[0];
        int   is_bi    = is_builtin_name(name);
        int   has_rdir = has_redirections(&pipeline[0]);

        if (is_bi && !has_rdir) {
            handle_builtin(pipeline[0].argv);
            return;
        }

        if (is_bi && has_rdir && is_parent_builtin(name)) {
            fprintf(stderr, YELLOW
                "myshell: warning: '%s' with I/O redirection runs in a "
                "subshell and will not affect the current shell\n" RESET,
                name);
        }

        pid_t pid = fork();
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            if (background) setpgid(0, 0);
            apply_redirections(&pipeline[0]);
            if (is_bi) { handle_builtin(pipeline[0].argv); exit(EXIT_SUCCESS); }
            execvp(name, pipeline[0].argv);
            fprintf(stderr, RED "myshell: %s: %s\n" RESET,
                    name, strerror(errno));
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            if (!background) {
                /* Retry on EINTR: Ctrl-C interrupts waitpid but the child
                   may catch SIGINT and keep running. */
                while (waitpid(pid, NULL, 0) < 0 && errno == EINTR)
                    ;
            } else {
                printf(YELLOW "[bg] pid %d\n" RESET, (int)pid);
            }
        } else {
            perror("fork");
        }
        return;
    }

    for (int i = 0; i < n; i++) {
        if (pipeline[i].argv[0] && is_parent_builtin(pipeline[i].argv[0])) {
            fprintf(stderr, YELLOW
                "myshell: warning: '%s' in a pipeline runs in a subshell "
                "and will not affect the current shell\n" RESET,
                pipeline[i].argv[0]);
        }
    }

    int   pipes[MAX_PIPELINE - 1][2];
    pid_t pids[MAX_PIPELINE];

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            for (int j = 0; j < i; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
    }

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            signal(SIGINT, SIG_DFL);
            if (background) setpgid(0, 0);

            if (i > 0)     dup2(pipes[i-1][0], STDIN_FILENO);
            if (i < n - 1) dup2(pipes[i][1],   STDOUT_FILENO);

            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            apply_redirections(&pipeline[i]);

            if (handle_builtin(pipeline[i].argv))
                exit(EXIT_SUCCESS);

            execvp(pipeline[i].argv[0], pipeline[i].argv);
            fprintf(stderr, RED "myshell: %s: %s\n" RESET,
                    pipeline[i].argv[0], strerror(errno));
            exit(EXIT_FAILURE);
        } else if (pids[i] < 0) {
            perror("fork");
        }
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (!background) {
        for (int i = 0; i < n; i++) {
            if (pids[i] > 0) {
                while (waitpid(pids[i], NULL, 0) < 0 && errno == EINTR)
                    ;
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
   MAIN LOOP
   ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    struct sigaction sa_chld = {0};
    sa_chld.sa_handler = handle_sigchld;
    sa_chld.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, NULL);

    struct sigaction sa_int = {0};
    sa_int.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa_int, NULL);

    using_history();
    rl_bind_key('\t', rl_complete);

    printf("\033[2J\033[H");
    print_banner();

    while (1) {
        if (sigint_received)
            process_sigint();

        char *prompt = build_prompt();
        char *line   = readline(prompt);
        free(prompt);

        if (sigint_received) {
            free(line);
            process_sigint();
            continue;
        }

        if (!line) {
            printf(YELLOW "\nBye!\n" RESET);
            break;
        }

        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') { free(line); continue; }

        size_t tlen = strlen(trimmed);
        while (tlen > 0 &&
               (trimmed[tlen-1] == ' ' || trimmed[tlen-1] == '\t'))
            trimmed[--tlen] = '\0';

        if (tlen == 0) { free(line); continue; }

        if (history_length > 0) {
            HIST_ENTRY *last = history_get(history_base + history_length - 1);
            if (!last || strcmp(last->line, trimmed) != 0)
                add_history(trimmed);
        } else {
            add_history(trimmed);
        }

        int background = 0;
        if (tlen > 0 && trimmed[tlen-1] == '&') {
            background = 1;
            trimmed[--tlen] = '\0';
            while (tlen > 0 && trimmed[tlen-1] == ' ')
                trimmed[--tlen] = '\0';
        }

        if (tlen == 0) { free(line); continue; }

        Cmd pipeline[MAX_PIPELINE];
        int n = parse_pipeline(trimmed, pipeline);

        if (n > 0 && pipeline[0].argv[0] != NULL)
            execute_pipeline(pipeline, n, background);

        free_pipeline(pipeline, n);
        free(line);
    }

    return 0;
}