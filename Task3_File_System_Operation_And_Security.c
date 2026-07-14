/* File System Operations and Security */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_USERS   50
#define MAX_UNAME   50
#define MAX_HASH    65
#define MAX_SALT    17
#define MAX_GROUP   32
#define USER_DB     "users.dat"
#define FILE_DB     "filedb.dat"
#define MAX_PATH    256
#define MAX_BUF     4096
#define MARKER      "SFMS"   /* 4-byte passphrase-verification marker */

static char current_user[MAX_UNAME]  = "";
static char current_group[MAX_GROUP] = "";
static int  logged_in = 0;


void generate_salt(char *out_salt) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    srand((unsigned int)time(NULL) ^ getpid());
    for (int i = 0; i < MAX_SALT - 1; i++)
        out_salt[i] = charset[rand() % (sizeof(charset) - 1)];
    out_salt[MAX_SALT - 1] = '\0';
}

void hash_password(const char *password, const char *salt, char *out_hash) {
    unsigned long hash = 5381;
    int c;
    char combined[MAX_UNAME + MAX_SALT];
    snprintf(combined, sizeof(combined), "%s%s", salt, password);
    const char *p = combined;
    while ((c = *p++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    snprintf(out_hash, MAX_HASH, "%lu", hash);
}

int user_exists(const char *username) {
    FILE *fp = fopen(USER_DB, "r");
    if (!fp) return 0;
    char line[256], uname[MAX_UNAME];
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%49[^:]:", uname);
        if (strcmp(uname, username) == 0) { fclose(fp); return 1; }
    }
    fclose(fp);
    return 0;
}

/* Registration: generate salt, hash password with it, store
 * "username:salt:hash:group" in the user database. */
int register_user(const char *username, const char *password) {
    if (strlen(username) == 0 || strlen(password) < 4) {
        printf("Username required, password must be >= 4 chars.\n");
        return -1;
    }
    if (user_exists(username)) {
        printf("Username already exists.\n");
        return -1;
    }
    char salt[MAX_SALT], hash[MAX_HASH];
    generate_salt(salt);
    hash_password(password, salt, hash);

    FILE *fp = fopen(USER_DB, "a");
    if (!fp) { perror("fopen"); return -1; }
    fprintf(fp, "%s:%s:%s:users\n", username, salt, hash);
    fclose(fp);

    chmod(USER_DB, S_IRUSR | S_IWUSR); /* owner-only access to user DB */
    printf("User '%s' registered successfully.\n", username);
    return 0;
}

/* Login: re-hash entered password with the STORED salt, compare
 * against the stored hash. */
int login_user(const char *username, const char *password) {
    FILE *fp = fopen(USER_DB, "r");
    if (!fp) { printf("No users registered yet.\n"); return -1; }

    char line[256], uname[MAX_UNAME], salt[MAX_SALT], stored_hash[MAX_HASH], group[MAX_GROUP];
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%49[^:]:%16[^:]:%64[^:]:%31[^\n]", uname, salt, stored_hash, group);
        if (strcmp(uname, username) == 0) {
            char computed_hash[MAX_HASH];
            hash_password(password, salt, computed_hash);
            if (strcmp(computed_hash, stored_hash) == 0) {
                fclose(fp);
                strncpy(current_user, username, MAX_UNAME - 1);
                strncpy(current_group, group, MAX_GROUP - 1);
                logged_in = 1;
                printf("Login successful. Welcome, %s!\n", username);
                return 0;
            }
            break;
        }
    }
    fclose(fp);
    printf("Invalid username or password.\n");
    return -1;
}

void logout_user(void) {
    printf("User '%s' logged out.\n", current_user);
    current_user[0] = '\0';
    current_group[0] = '\0';
    logged_in = 0;
}

int require_login(void) {
    if (!logged_in) { printf("Error: You must log in first.\n"); return 0; }
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
