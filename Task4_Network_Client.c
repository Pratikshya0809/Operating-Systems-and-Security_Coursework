/* Task 4: Network Programming and IPC */

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
    char username[64], password[64];

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect"); close(sock_fd); exit(EXIT_FAILURE);
    }
    printf("Connected to server %s:%d\n", SERVER_IP, PORT);

    /* ---- Authentication phase ---- */
    printf("Username: "); scanf("%63s", username);
    printf("Password: "); scanf("%63s", password);

    snprintf(buffer, sizeof(buffer), "AUTH %s %s", username, password);
    write(sock_fd, buffer, strlen(buffer));

    int bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
    buffer[bytes_read] = '\0';

    if (strcmp(buffer, "AUTH_OK") != 0) {
        printf("Authentication failed. Disconnecting.\n");
        close(sock_fd);
        return 1;
    }
    printf("Authenticated successfully!\n");

    /* ---- Data exchange phase ---- */
    printf("Type messages (they'll be sent as MSG <text>), or QUIT to exit:\n");
    getchar(); /* consume leftover newline from scanf */

    while (1) {
        printf("> ");
        if (!fgets(buffer, BUF_SIZE, stdin)) break;
        buffer[strcspn(buffer, "\n")] = '\0';
        if (strlen(buffer) == 0) continue;

        char msg[BUF_SIZE];
        if (strcmp(buffer, "QUIT") == 0) {
            write(sock_fd, "QUIT", 4);
            bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
            buffer[bytes_read] = '\0';
            printf("Server: %s\n", buffer);
            break;
        }

        snprintf(msg, sizeof(msg), "MSG %s", buffer);
        write(sock_fd, msg, strlen(msg));

        bytes_read = read(sock_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) { printf("Server closed connection.\n"); break; }
        buffer[bytes_read] = '\0';
        printf("Server: %s\n", buffer);
    }

    close(sock_fd);
    return 0;
}
