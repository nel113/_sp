#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// ---------------------------------------------------------------------------
// Demonstrates fork(), execvp(), and wait()
//   - fork()  creates a child process that is a copy of the parent
//   - execvp() replaces the child's process image with a new program
//   - wait()  makes the parent wait for the child to finish
// ---------------------------------------------------------------------------

int main() {
    pid_t pid;

    printf("[PARENT] PID = %d | About to call fork()\n", getpid());

    pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        // ---- CHILD process ----
        printf("[CHILD]  PID = %d | My parent is PID = %d\n", getpid(), getppid());

        // Replace this process image with /bin/ls
        char *args[] = {"ls", "-la", NULL};
        printf("[CHILD]  About to execvp('ls', '-la')\n");

        execvp("ls", args);

        // execvp returns only on error
        perror("execvp failed");
        exit(1);
    } else {
        // ---- PARENT process ----
        printf("[PARENT] Forked child with PID = %d\n", pid);

        // Wait for the child to complete
        int status;
        wait(&status);

        if (WIFEXITED(status)) {
            printf("[PARENT] Child exited with status %d\n", WEXITSTATUS(status));
        }
    }

    return 0;
}
