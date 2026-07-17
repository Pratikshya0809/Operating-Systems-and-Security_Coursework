/* Task 4: Network Programming and IPC */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT       8080
#define BUF_SIZE   1024
#define MAX_MSG_LEN 512

#define VALID_USER "admin"
#define VALID_PASS "admin123"

static int server_fd = -1;
static volatile sig_atomic_t running = 1;

/* Graceful shutdown on Ctrl+C */
void handle_sigint(int sig) {
    (void)sig;
    printf("\nShutdown signal received. Closing server...\n");
    running = 0;
    if (server_fd >= 0) close(server_fd); /* unblocks accept() */
}

/* Reject control characters / non-printable input to prevent
 * injection of unexpected protocol-breaking bytes. */
int is_valid_input(const char *str, int len) {
    if (len <= 0 || len > MAX_MSG_LEN) return 0;
    for (int i = 0; i < len; i++) {
        if (!isprint((unsigned char)str[i]) && str[i] != '\0') return 0;
    }
    return 1;
}

int authenticate(const char *msg) {
    char cmd[16], user[64], pass[64];
    int n = sscanf(msg, "%15s %63s %63s", cmd, user, pass);
    if (n != 3 || strcmp(cmd, "AUTH") != 0) return 0;
    return (strcmp(user, VALID_USER) == 0 && strcmp(pass, VALID_PASS) == 0);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    char buffer[BUF_SIZE];
    pthread_t tid = pthread_self();

    printf("[Thread %lu] Client handler started.\n", (unsigned long)tid);

    int authed = 0;
    int attempts = 0;
    const int MAX_ATTEMPTS = 3;

    /* Allow limited auth retries before disconnecting -- basic
     * brute-force mitigation. */
    while (attempts < MAX_ATTEMPTS && !authed) {
        int bytes_read = read(client_fd, buffer, BUF_SIZE - 1);
        if (bytes_read <= 0) {
            printf("[Thread %lu] Client disconnected during auth.\n", (unsigned long)tid);
            close(client_fd);
            return NULL;
        }
        buffer[bytes_read] = '\0';

        if (!is_valid_input(buffer, bytes_read)) {
            write(client_fd, "ERR invalid input", 18);
            attempts++;
            continue;
        }

        if (authenticate(buffer)) {
            authed = 1;
            write(client_fd, "AUTH_OK", 7);
            printf("[Thread %lu] Client authenticated.\n", (unsigned long)tid);
        } else {
            attempts++;
            write(client_fd, "AUTH_FAIL", 9);
            printf("[Thread %lu] Auth attempt %d/%d failed.\n",
                   (unsigned long)tid, attempts, MAX_ATTEMPTS);
        }
    }

    if (!authed) {
        write(client_fd, "ERR too many failed attempts", 29);
        printf("[Thread %lu] Max auth attempts exceeded. Disconnecting.\n", (unsigned long)tid);
        close(client_fd);
        return NULL;
    }

    /* ---- Data exchange phase ---- */
    int bytes_read;
    while (running && (bytes_read = read(client_fd, buffer, BUF_SIZE - 1)) > 0) {
        buffer[bytes_read] = '\0';

        if (!is_valid_input(buffer, bytes_read)) {
            write(client_fd, "ERR invalid input", 18);
            printf("[Thread %lu] Rejected invalid/malformed input.\n", (unsigned long)tid);
            continue;
        }

        printf("[Thread %lu] Received: %s\n", (unsigned long)tid, buffer);

        if (strncmp(buffer, "QUIT", 4) == 0) {
            write(client_fd, "BYE", 3);
            printf("[Thread %lu] Client requested disconnect.\n", (unsigned long)tid);
            break;
        }

        if (strncmp(buffer, "MSG ", 4) == 0) {
            char response[BUF_SIZE];
            snprintf(response, sizeof(response), "ACK %s", buffer + 4);
            if (write(client_fd, response, strlen(response)) < 0) {
                perror("write");
                break;
            }
        } else {
            write(client_fd, "ERR unknown command", 20);
        }
    }

    if (bytes_read < 0) perror("read");

    close(client_fd);
    printf("[Thread %lu] Connection closed.\n", (unsigned long)tid);
    return NULL;
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    signal(SIGINT, handle_sigint);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d. Press Ctrl+C to stop.\n", PORT);

    while (running) {
        int *client_fd = malloc(sizeof(int));
        if (!client_fd) { perror("malloc"); continue; }

        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (*client_fd < 0) {
            if (!running) { free(client_fd); break; } /* interrupted by shutdown */
            perror("accept");
            free(client_fd);
            continue;
        }

        printf("New client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
            continue;
        }
        pthread_detach(tid);
    }

    if (server_fd >= 0) close(server_fd);
    printf("Server shut down cleanly.\n");
    return 0;
}
