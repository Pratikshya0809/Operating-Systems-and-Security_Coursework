/*
 * Task 4: Network Programming and IPC
 * client.c — TCP client (Day 2: basic connect, send, receive) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SERVER_IP  "127.0.0.1"
#define PORT       8080
#define BUF_SIZE   1024

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUF_SIZE];

    /* 1. Create TCP socket */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* 2. Set up server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    /* 3. Connect to server */
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server %s:%d\n", SERVER_IP, PORT);
    printf("Type messages to send ('exit' to quit):\n");

    /* 4. Send/receive loop */
    while (1) {
        printf("> ");
        if (!fgets(buffer, BUF_SIZE, stdin)) break;
        buffer[strcspn(buffer, "\n")] = '\0'; /* strip trailing newline */

        if (strlen(buffer) == 0) continue;

        write(sock_fd, buffer, strlen(buffer));

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Disconnecting...\n");
            break;
        }

        int bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            printf("Server closed connection.\n");
            break;
        }
        buffer[bytes_read] = '\0';
        printf("Server echoed: %s\n", buffer);
    }

    close(sock_fd);
    return 0;
}
