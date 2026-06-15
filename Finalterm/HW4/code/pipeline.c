#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>

// Demonstrates: fork, exec, wait, pipe, dup2
// Equivalent to: ls | sort

int main(void) {
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    pid_t pid1 = fork();
    if (pid1 == 0) {
        // Child 1: ls → writes to pipe
        close(pipefd[0]);                    // close read end
        dup2(pipefd[1], STDOUT_FILENO);      // stdout → pipe write end
        close(pipefd[1]);

        execlp("ls", "ls", (char *)NULL);
        perror("execlp ls");
        _exit(127);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        // Child 2: sort ← reads from pipe
        close(pipefd[1]);                    // close write end
        dup2(pipefd[0], STDIN_FILENO);       // stdin ← pipe read end
        close(pipefd[0]);

        execlp("sort", "sort", (char *)NULL);
        perror("execlp sort");
        _exit(127);
    }

    // Parent: close both ends and wait
    close(pipefd[0]);
    close(pipefd[1]);

    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    return 0;
}
