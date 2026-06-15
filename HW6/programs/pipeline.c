#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// ---------------------------------------------------------------------------
// Builds a shell pipeline:  ls | wc -l
//
// This is equivalent to running in a shell:
//     $ ls | wc -l
//
// The pipeline is constructed as follows:
//
//   [ls] --(stdout)--> pipe[1] --(stdin)--> [wc -l]
//
// Step-by-step:
//   1. pipe()   creates a unidirectional data channel (two fds: read end, write end)
//   2. fork()   creates two child processes
//   3. dup2()   remaps stdin/stdout to the pipe
//   4. close()  releases unused pipe ends in each process
//   5. execvp() replaces each child with the target command
//   6. wait()   parent waits for both children to finish
// ---------------------------------------------------------------------------

int main() {
    int pipefd[2];   // pipefd[0] = read end, pipefd[1] = write end

    if (pipe(pipefd) < 0) {
        perror("pipe failed");
        exit(1);
    }

    printf("[PARENT] Created pipe: read_fd=%d, write_fd=%d\n", pipefd[0], pipefd[1]);

    // --- First child: executes "ls" (produces data) -------------------------
    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("fork 1 failed");
        exit(1);
    }

    if (pid1 == 0) {
        // CHILD #1: redirect stdout to the write end of the pipe
        close(pipefd[0]);                    // not reading from the pipe
        dup2(pipefd[1], STDOUT_FILENO);      // fd 1  -> write end of pipe
        close(pipefd[1]);                    // original fd no longer needed

        char *args[] = {"ls", NULL};
        execvp("ls", args);
        perror("execvp ls failed");
        exit(1);
    }

    // --- Second child: executes "wc -l" (consumes data) ---------------------
    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("fork 2 failed");
        exit(1);
    }

    if (pid2 == 0) {
        // CHILD #2: redirect stdin to the read end of the pipe
        close(pipefd[1]);                    // not writing to the pipe
        dup2(pipefd[0], STDIN_FILENO);       // fd 0  <- read end of pipe
        close(pipefd[0]);                    // original fd no longer needed

        char *args[] = {"wc", "-l", NULL};
        execvp("wc", args);
        perror("execvp wc failed");
        exit(1);
    }

    // --- Parent: close both ends and wait for children ----------------------
    // The parent must close both ends so the pipe actually closes when both
    // children exit. Otherwise the reader would hang waiting for more data.
    close(pipefd[0]);
    close(pipefd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    printf("\n[PARENT] Pipeline finished.\n");
    return 0;
}
