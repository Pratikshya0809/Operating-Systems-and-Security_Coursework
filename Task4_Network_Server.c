/*
 * Task 4: Network Programming and IPC
 * server.c — TCP server (Day 1: basic socket, single client) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT       8080
#define BUF_SIZE   1024

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];

    /* 1. Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    /* Allow quick restart of server on same port (avoid "Address already in use") */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 2. Bind to address/port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* 3. Listen for incoming connections */
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    /* 4. Accept a single client connection (Day 1 scope) */
    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* 5. Simple echo loop */
    int bytes_read;
    while ((bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("Received: %s\n", buffer);

        if (strncmp(buffer, "exit", 4) == 0) {
            printf("Client requested disconnect.\n");
            break;
        }

        write(client_fd, buffer, bytes_read); /* echo back */
    }

    close(client_fd);
    close(server_fd);
    printf("Server shut down.\n");
    return 0;
}
