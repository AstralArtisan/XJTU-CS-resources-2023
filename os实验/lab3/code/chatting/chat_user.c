#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

#define DEVICE_PATH "/dev/chatdev"
#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    int fd;
    char name[32];
    char read_buf[BUF_SIZE];
    char write_buf[BUF_SIZE];
    char msg_buf[BUF_SIZE + 32];
    fd_set fds;
    int max_fd;

    if (argc < 2) {
        printf("Usage: %s <username>\n", argv[0]);
        return 1;
    }
    strncpy(name, argv[1], 31);
    name[31] = '\0';

    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    printf("Welcome to the chat room, %s!\n", name);
    printf("Type your message and press Enter.\n");

    while (1) {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(fd, &fds);

        max_fd = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;

        int ret = select(max_fd + 1, &fds, NULL, NULL, NULL);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // Check for user input
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            if (fgets(write_buf, BUF_SIZE, stdin) == NULL) {
                break; // EOF
            }
            // Remove newline
            write_buf[strcspn(write_buf, "\n")] = 0;

            if (strlen(write_buf) > 0) {
                if (strncmp(write_buf, "/w ", 3) == 0) {
                    char target[32], content[BUF_SIZE];
                    if (sscanf(write_buf + 3, "%31s %255[^\n]", target, content) == 2) {
                        snprintf(msg_buf, sizeof(msg_buf), "[%s->%s] %s\n", name, target, content);
                        write(fd, msg_buf, strlen(msg_buf));
                    } else {
                        printf("Usage: /w <user> <message>\n");
                    }
                } else {
                    snprintf(msg_buf, sizeof(msg_buf), "[%s] %s\n", name, write_buf);
                    write(fd, msg_buf, strlen(msg_buf));
                }
            }
        }

        // Check for device output
        if (FD_ISSET(fd, &fds)) {
            int n = read(fd, read_buf, sizeof(read_buf) - 1);
            if (n > 0) {
                read_buf[n] = '\0';
                
                // Check for private message marker
                if (strstr(read_buf, "->")) {
                    char my_target[64];
                    char my_sender[64];
                    snprintf(my_target, sizeof(my_target), "->%s]", name);
                    snprintf(my_sender, sizeof(my_sender), "[%s->", name);
                    
                    // Show if I am the target OR I am the sender
                    if (strstr(read_buf, my_target) || strstr(read_buf, my_sender)) {
                        printf("%s", read_buf);
                        fflush(stdout);
                    }
                } else {
                    // Broadcast message
                    char my_prefix[64];
                    snprintf(my_prefix, sizeof(my_prefix), "[%s]", name);
                    // Don't echo my own broadcast messages
                    if (strstr(read_buf, my_prefix) != read_buf) {
                        printf("%s", read_buf);
                        fflush(stdout);
                    }
                }
            } else if (n < 0) {
                perror("read");
                break;
            }
        }
    }

    close(fd);
    return 0;
}
