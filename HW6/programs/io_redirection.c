#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Demonstrates dup2() – duplicate a file descriptor onto a target fd
//
//   dup2(oldfd, newfd) closes newfd (if open) and makes it point to the same
//   file/pipe as oldfd. This is the core mechanism behind shell redirection:
//
//     $ ls > output.txt          -->  dup2(fd, STDOUT_FILENO)
//     $ sort < input.txt         -->  dup2(fd, STDIN_FILENO)
//     $ program 2> errors.log    -->  dup2(fd, STDERR_FILENO)
//
// We use write() (not printf) during the redirected phase to avoid stdio
// buffering confusion, since printf's FILE* may cache the old fd.
// ---------------------------------------------------------------------------

int main() {
    const char *outfile = "/tmp/hw6_redirect_out.txt";

    // -----------------------------------------------------------------------
    // Example 1: Redirect stdout to a file (equivalent to "cmd > file")
    // -----------------------------------------------------------------------
    printf("=== Example 1: Redirecting stdout to '%s' ===\n", outfile);

    int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open failed"); exit(1); }

    fflush(stdout);                              // flush before redirect
    int saved_stdout = dup(STDOUT_FILENO);        // save original stdout

    dup2(fd, STDOUT_FILENO);                     // fd 1 -> file
    close(fd);

    // Use write() directly so there is no stdio buffering interference
    const char *msg1 = "This line went to the file, not the terminal!\n";
    const char *msg2 = "Another line written to the file.\n";
    write(STDOUT_FILENO, msg1, strlen(msg1));
    write(STDOUT_FILENO, msg2, strlen(msg2));

    dup2(saved_stdout, STDOUT_FILENO);           // restore original stdout
    close(saved_stdout);

    printf("[INFO] Wrote to file. Now reading it back:\n\n");
    fflush(stdout);

    // -----------------------------------------------------------------------
    // Read the file back and display it on stdout
    // -----------------------------------------------------------------------
    fd = open(outfile, O_RDONLY);
    if (fd < 0) { perror("open (read) failed"); exit(1); }

    char buffer[512];
    ssize_t n;
    while ((n = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[n] = '\0';
        write(STDOUT_FILENO, buffer, n);
    }
    close(fd);

    // -----------------------------------------------------------------------
    // Example 2: Redirect stderr to a log file (equivalent to "cmd 2> err.log")
    // -----------------------------------------------------------------------
    printf("\n=== Example 2: Redirecting stderr to a log file ===\n");

    const char *errfile = "/tmp/hw6_redirect_err.txt";
    int efd = open(errfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (efd < 0) { perror("open (stderr) failed"); exit(1); }

    fflush(stderr);
    int saved_stderr = dup(STDERR_FILENO);

    dup2(efd, STDERR_FILENO);                    // fd 2 -> log file
    close(efd);

    const char *err1 = "ERROR: Something went wrong!\n";
    const char *err2 = "DEBUG: More diagnostics here...\n";
    write(STDERR_FILENO, err1, strlen(err1));
    write(STDERR_FILENO, err2, strlen(err2));

    dup2(saved_stderr, STDERR_FILENO);           // restore original stderr
    close(saved_stderr);

    printf("[INFO] Errors written to '%s'\n", errfile);

    return 0;
}
