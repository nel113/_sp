#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BUFFER_SIZE 256

// ---------------------------------------------------------------------------
// Demonstrates open(), close(), read(), and write()
//   - open()   opens a file descriptor (returns FD >= 0 on success, -1 on error)
//   - read()   reads raw bytes from a file descriptor into a buffer
//   - write()  writes raw bytes from a buffer to a file descriptor
//   - close()  releases the file descriptor
// ---------------------------------------------------------------------------

int main() {
    int fd;
    const char *filename = "/tmp/hw6_demo_file.txt";
    const char *original_text = "Hello from HW6 file I/O demo!\n"
                                "Line 2: system calls are powerful.\n"
                                "Line 3: open, read, write, close.\n";

    // --- 1. open(): create (or truncate) the file for writing ---------------
    // O_WRONLY  = write only
    // O_CREAT   = create file if it doesn't exist
    // O_TRUNC   = truncate to 0 length if it already exists
    // 0644      = permissions: rw-r--r--
    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open (write) failed");
        exit(1);
    }
    printf("[INFO] Opened '%s' for writing (fd = %d)\n", filename, fd);

    // --- 2. write(): write the data ----------------------------------------
    ssize_t bytes_written = write(fd, original_text, strlen(original_text));
    if (bytes_written < 0) {
        perror("write failed");
        close(fd);
        exit(1);
    }
    printf("[INFO] Wrote %zd bytes to fd %d\n", bytes_written, fd);

    // --- 3. close(): release the fd ----------------------------------------
    if (close(fd) < 0) {
        perror("close failed");
        exit(1);
    }
    printf("[INFO] Closed fd %d\n\n", fd);

    // --- 4. open(): reopen for reading -------------------------------------
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open (read) failed");
        exit(1);
    }
    printf("[INFO] Reopened '%s' for reading (fd = %d)\n\n", filename, fd);

    // --- 5. read(): loop-read and write to stdout --------------------------
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    printf("--- File contents (read via fd %d) ---\n", fd);
    fflush(stdout);
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';                    // null-terminate for printf
        write(STDOUT_FILENO, buffer, bytes_read);     // write to stdout (fd 1)
    }
    if (bytes_read < 0) {
        perror("read failed");
    }
    printf("--- End of file ---\n");

    close(fd);
    return 0;
}
