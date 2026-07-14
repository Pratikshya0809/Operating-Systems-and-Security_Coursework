/* File System Operations and Security */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_USERS 50
#define MAX_UNAME 50
#define MAX_HASH  65
#define USER_DB   "users.dat"
#define MAX_LOGGED_IN 1

typedef struct {
    char username[MAX_UNAME];
    char password_hash[MAX_HASH];
} User;

static char current_user[MAX_UNAME] = "";
static int  logged_in = 0;

void hash_password(const char *password, char *out_hash) {
    unsigned long hash = 5381;
    int c;
    const char *p = password;
    while ((c = *p++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    snprintf(out_hash, MAX_HASH, "%lu", hash);
}

/* Check if a username already exists in the user DB */
int user_exists(const char *username) {
    FILE *fp = fopen(USER_DB, "r");
    if (!fp) return 0;
    User u;
    while (fread(&u, sizeof(User), 1, fp) == 1) {
        if (strcmp(u.username, username) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

/* Register a new user */
int register_user(const char *username, const char *password) {
    if (strlen(username) == 0 || strlen(password) < 4) {
        printf("Username required, password must be >= 4 chars.\n");
        return -1;
    }
    if (user_exists(username)) {
        printf("Username already exists.\n");
        return -1;
    }
    User u;
    strncpy(u.username, username, MAX_UNAME - 1);
    u.username[MAX_UNAME - 1] = '\0';
    hash_password(password, u.password_hash);

    FILE *fp = fopen(USER_DB, "a");
    if (!fp) { perror("fopen"); return -1; }
    fwrite(&u, sizeof(User), 1, fp);
    fclose(fp);

    /* Restrict user DB so only the owner can read/write it */
    chmod(USER_DB, S_IRUSR | S_IWUSR);
    printf("User '%s' registered successfully.\n", username);
    return 0;
}

/* Login: verify credentials against user DB */
int login_user(const char *username, const char *password) {
    FILE *fp = fopen(USER_DB, "r");
    if (!fp) {
        printf("No users registered yet.\n");
        return -1;
    }
    char hash[MAX_HASH];
    hash_password(password, hash);

    User u;
    while (fread(&u, sizeof(User), 1, fp) == 1) {
        if (strcmp(u.username, username) == 0 &&
            strcmp(u.password_hash, hash) == 0) {
            fclose(fp);
            strncpy(current_user, username, MAX_UNAME - 1);
            logged_in = 1;
            printf("Login successful. Welcome, %s!\n", username);
            return 0;
        }
    }
    fclose(fp);
    printf("Invalid username or password.\n");
    return -1;
}

void logout_user(void) {
    printf("User '%s' logged out.\n", current_user);
    current_user[0] = '\0';
    logged_in = 0;
}

int require_login(void) {
    if (!logged_in) {
        printf("Error: You must log in first.\n");
        return 0;
    }
    return 1;
}

/* Temporary main so this compiles standalone for Commit 1 */
#ifndef SFMS_FULL_BUILD
int main() {
    char uname[MAX_UNAME], pass[MAX_UNAME];
    int choice;
    printf("1. Register\n2. Login\nChoice: ");
    scanf("%d", &choice);
    printf("Username: "); scanf("%s", uname);
    printf("Password: "); scanf("%s", pass);
    if (choice == 1) register_user(uname, pass);
    else if (choice == 2) login_user(uname, pass);
    return 0;
}
#endif
