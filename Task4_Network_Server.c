/* Network Programming and IPC*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT       8080
#define BUF_SIZE   1024

/* Hardcoded demo credentials. */
#define VALID_USER "admin"
#define VALID_PASS "admin123"

int authenticate(const char *msg) {
    char cmd[16], user[64], pass[64];
    int n = sscanf(msg, "%15s %63s %63s", cmd, user, pass);
    if (n != 3 || strcmp(cmd, "AUTH") != 0) return 0;
    return (strcmp(user, VALID_USER) == 0 && strcmp(pass, VALID_PASS) == 0);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUF_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind"); close(server_fd); exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0) {
        perror("listen"); close(server_fd); exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) { perror("accept"); close(server_fd); exit(EXIT_FAILURE); }
    printf("Client connected: %s:%d\n",
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    /* ---- Authentication phase (protocol requires this first) ---- */
    int authed = 0;
    int bytes_read = read(client_fd, buffer, BUF_SIZE - 1);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        if (authenticate(buffer)) {
            authed = 1;
            write(client_fd, "AUTH_OK", 7);
            printf("Client authenticated successfully.\n");
        } else {
            write(client_fd, "AUTH_FAIL", 9);
            printf("Client authentication failed.\n");
        }
    }

    /* ---- Data exchange phase (only if authenticated) ---- */
    if (authed) {
        while ((bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("Received: %s\n", buffer);

            if (strncmp(buffer, "QUIT", 4) == 0) {
                write(client_fd, "BYE", 3);
                printf("Client requested disconnect.\n");
                break;
            }

            if (strncmp(buffer, "MSG ", 4) == 0) {
                char response[BUF_SIZE];
                snprintf(response, sizeof(response), "ACK %s", buffer + 4);
                write(client_fd, response, strlen(response));
            } else {
                write(client_fd, "ERR unknown command", 20);
            }
        }
    }

    close(client_fd);
    close(server_fd);
    printf("Server shut down.\n");
    return 0;
}
