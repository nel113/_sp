#include <unistd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <signal.h>

// Demonstrates: raw syscall(2), basic signal handling

volatile sig_atomic_t gotsig = 0;

static void handler(int sig) {
    gotsig = sig;
}

int main(void) {
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Write using raw syscall (bypasses libc)
    const char msg[] = "PID: ";
    syscall(SYS_write, STDOUT_FILENO, msg, sizeof(msg) - 1);

    pid_t pid = syscall(SYS_getpid);
    char pidbuf[16];
    int len = 0;
    do {
        pidbuf[len++] = '0' + (pid % 10);
        pid /= 10;
    } while (pid);
    for (int i = len - 1; i >= 0; i--)
        syscall(SYS_write, STDOUT_FILENO, &pidbuf[i], 1);

    const char nl[] = "\nWaiting for SIGINT or SIGTERM...\n";
    syscall(SYS_write, STDOUT_FILENO, nl, sizeof(nl) - 1);

    while (!gotsig)
        pause();  // wait for signal

    const char end[] = "Caught signal, exiting.\n";
    syscall(SYS_write, STDOUT_FILENO, end, sizeof(end) - 1);

    syscall(SYS_exit, 0);
    return 0;
}
