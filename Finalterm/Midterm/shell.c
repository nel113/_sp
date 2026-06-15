#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_LINE    1024
#define MAX_ARGS    128
#define MAX_PIPES   16
#define TOK_DELIM   " \t\r\n"

static volatile sig_atomic_t interrupted = 0;

static void sigint_handler(int sig) {
    (void)sig;
    interrupted = 1;
    write(STDOUT_FILENO, "\n", 1);
}

static void setup_signals(void) {
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGQUIT, SIG_IGN);
}

static char *read_line(void) {
    char *line = malloc(MAX_LINE);
    if (!line) { perror("malloc"); exit(EXIT_FAILURE); }

    if (!fgets(line, MAX_LINE, stdin)) {
        free(line);
        return NULL;
    }

    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
        line[len - 1] = '\0';
    return line;
}

static int parse_pipe(char *line, char **commands) {
    int count = 0;
    commands[count] = strtok(line, "|");
    while (commands[count] && count < MAX_PIPES - 1)
        commands[++count] = strtok(NULL, "|");
    return count;
}

static int parse_redirect(char *cmd, char **args,
                          char **infile, char **outfile) {
    *infile = NULL;
    *outfile = NULL;
    int i = 0;
    char *token = strtok(cmd, TOK_DELIM);

    while (token && i < MAX_ARGS - 1) {
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, TOK_DELIM);
            if (token) *infile = token;
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, TOK_DELIM);
            if (token) *outfile = token;
        } else {
            args[i++] = token;
        }
        token = strtok(NULL, TOK_DELIM);
    }
    args[i] = NULL;
    return i;
}

static int run_builtin(char **args) {
    if (args[0] == NULL) return 1;

    if (strcmp(args[0], "exit") == 0) {
        exit(EXIT_SUCCESS);
    } else if (strcmp(args[0], "cd") == 0) {
        const char *dir = args[1] ? args[1] : getenv("HOME");
        if (!dir) dir = "/";
        if (chdir(dir) != 0)
            perror("cd");
        return 1;
    } else if (strcmp(args[0], "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)))
            printf("%s\n", cwd);
        else
            perror("pwd");
        return 1;
    } else if (strcmp(args[0], "help") == 0) {
        printf("MiniShell — built-in commands:\n"
               "  cd [dir]      change directory\n"
               "  pwd           print working directory\n"
               "  exit          quit the shell\n"
               "  help          show this help\n"
               "\nSupported syntax:\n"
               "  cmd < infile           redirect stdin\n"
               "  cmd > outfile          redirect stdout\n"
               "  cmd1 | cmd2            pipe stdout -> stdin\n"
               "\n");
        return 1;
    }
    return 0;
}

static int execute_external(char **args, char *infile, char *outfile,
                            int pipe_in, int pipe_out) {
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return -1; }

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (pipe_in >= 0) {
            dup2(pipe_in, STDIN_FILENO);
            close(pipe_in);
        }
        if (pipe_out >= 0) {
            dup2(pipe_out, STDOUT_FILENO);
            close(pipe_out);
        }

        if (infile) {
            int fd = open(infile, O_RDONLY);
            if (fd < 0) { perror(infile); exit(EXIT_FAILURE); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (outfile) {
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(outfile); exit(EXIT_FAILURE); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(args[0], args);
        fprintf(stderr, "%s: command not found\n", args[0]);
        exit(EXIT_FAILURE);
    }

    return pid;
}

static int run_command(char *cmd, int pipe_in, int pipe_out) {
    char *args[MAX_ARGS];
    char *infile, *outfile;
    parse_redirect(cmd, args, &infile, &outfile);

    if (run_builtin(args))
        return -1;

    return execute_external(args, infile, outfile, pipe_in, pipe_out);
}

static void run_pipeline(char *line) {
    char *commands[MAX_PIPES];
    int n = parse_pipe(line, commands);
    if (n == 0) return;

    if (n == 1) {
        run_command(commands[0], -1, -1);
        return;
    }

    pid_t pids[MAX_PIPES];
    int pipefd[2];
    int prev_fd = -1;

    for (int i = 0; i < n; i++) {
        if (i < n - 1) {
            if (pipe(pipefd) < 0) { perror("pipe"); return; }
        }

        int r = run_command(commands[i], prev_fd,
                            (i < n - 1) ? pipefd[1] : -1);
        if (r > 0) pids[i] = r;

        if (prev_fd >= 0) close(prev_fd);
        if (i < n - 1) {
            close(pipefd[1]);
            prev_fd = pipefd[0];
        }
    }

    for (int i = 0; i < n; i++) {
        if (pids[i] > 0) waitpid(pids[i], NULL, 0);
    }
}

static void repl(void) {
    char *line;
    setup_signals();

    while (1) {
        if (interrupted) {
            interrupted = 0;
            printf("\n");
        }

        char cwd[256];
        if (getcwd(cwd, sizeof(cwd)))
            printf("\033[1;32m%s\033[0m> ", cwd);
        else
            printf("> ");

        fflush(stdout);
        line = read_line();

        if (!line) {
            printf("exit\n");
            break;
        }

        if (strlen(line) > 0)
            run_pipeline(line);

        free(line);
    }
}

int main(void) {
    printf("MiniShell — System Programming Midterm Project\n"
           "Type 'help' for available commands.\n\n");
    repl();
    return 0;
}
