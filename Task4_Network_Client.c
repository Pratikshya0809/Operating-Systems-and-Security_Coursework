/* Task 4: Network Programming and IPC */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP   "127.0.0.1"
#define PORT        8080
#define BUF_SIZE    1024
#define MAX_MSG_LEN 512

int is_valid_input(const char *str, int len) {
    if (len <= 0 || len > MAX_MSG_LEN) return 0;
    for (int i = 0; i < len; i++) {
        if (!isprint((unsigned char)str[i]) && str[i] != '\0') return 0;
    }
    return 1;
}

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];
    char username[64], password[64];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server address.\n");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        fprintf(stderr, "Could not reach server at %s:%d. Is it running?\n", SERVER_IP, PORT);
        close(sock_fd);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    printf("Username: ");
    if (!scanf("%63s", username)) { fprintf(stderr, "Input error.\n"); close(sock_fd); return 1; }
    printf("Password: ");
    if (!scanf("%63s", password)) { fprintf(stderr, "Input error.\n"); close(sock_fd); return 1; }

    if (strlen(username) == 0 || strlen(password) == 0) {
        fprintf(stderr, "Username/password cannot be empty.\n");
        close(sock_fd);
        return 1;
    }

    snprintf(buffer, sizeof(buffer), "AUTH %s %s", username, password);
    if (write(sock_fd, buffer, strlen(buffer)) < 0) {
        perror("write"); close(sock_fd); return 1;
    }

    int bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
    if (bytes_read <= 0) {
        fprintf(stderr, "Server closed connection unexpectedly during auth.\n");
        close(sock_fd);
        return 1;
    }
    buffer[bytes_read] = '\0';

    if (strncmp(buffer, "AUTH_OK", 7) != 0) {
        printf("Authentication failed: %s\n", buffer);
        close(sock_fd);
        return 1;
    }
    printf("Authenticated successfully!\n");

    printf("Type messages (QUIT to exit):\n");
    getchar();

    while (1) {
        printf("> ");
        if (!fgets(buffer, BUF_SIZE, stdin)) {
            printf("\nInput closed. Exiting.\n");
            break;
        }
        buffer[strcspn(buffer, "\n")] = '\0';
        int len = strlen(buffer);

        if (len == 0) continue;
        if (!is_valid_input(buffer, len)) {
            printf("Invalid input (non-printable characters or too long). Try again.\n");
            continue;
        }

        char msg[BUF_SIZE];
        if (strcmp(buffer, "QUIT") == 0) {
            write(sock_fd, "QUIT", 4);
            bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                printf("Server: %s\n", buffer);
            }
            break;
        }

        snprintf(msg, sizeof(msg), "MSG %s", buffer);
        if (write(sock_fd, msg, strlen(msg)) < 0) {
            perror("write");
            break;
        }

        bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read < 0) {
            perror("read");
            break;
        }
        if (bytes_read == 0) {
            printf("Server closed the connection.\n");
            break;
        }
        buffer[bytes_read] = '\0';
        printf("Server: %s\n", buffer);
    }

    close(sock_fd);
    printf("Disconnected.\n");
    return 0;
}
