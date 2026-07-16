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


/* FILE OPERATION WITH METADATA + PERMISSION CHECKING */

typedef struct {
    char filename[MAX_PATH];
    char owner[MAX_UNAME];
    char group[MAX_GROUP];
    int  mode;       /* e.g. 0640 - owner rw-, group r--, others --- */
    int  encrypted;  /* 0 = plaintext, 1 = encrypted on disk */
} FileMeta;

/* Look up a file's metadata record. Returns 0 on success. */
int get_file_meta(const char *filename, FileMeta *out) {
    FILE *fp = fopen(FILE_DB, "r");
    if (!fp) return -1;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        FileMeta m;
        int mode;
        sscanf(line, "%255[^:]:%49[^:]:%31[^:]:%o:%d",
               m.filename, m.owner, m.group, &mode, &m.encrypted);
        m.mode = mode;
        if (strcmp(m.filename, filename) == 0) {
            *out = m;
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

/* Rewrite the whole metadata DB, replacing/adding one record. */
int save_file_meta(FileMeta *rec) {
    FileMeta all[512];
    int count = 0;
    FILE *fp = fopen(FILE_DB, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp) && count < 512) {
            FileMeta m; int mode;
            sscanf(line, "%255[^:]:%49[^:]:%31[^:]:%o:%d",
                   m.filename, m.owner, m.group, &mode, &m.encrypted);
            m.mode = mode;
            if (strcmp(m.filename, rec->filename) != 0) all[count++] = m;
        }
        fclose(fp);
    }
    all[count++] = *rec;

    fp = fopen(FILE_DB, "w");
    if (!fp) { perror("save_file_meta"); return -1; }
    for (int i = 0; i < count; i++)
        fprintf(fp, "%s:%s:%s:%04o:%d\n", all[i].filename, all[i].owner,
                all[i].group, all[i].mode, all[i].encrypted);
    fclose(fp);
    chmod(FILE_DB, S_IRUSR | S_IWUSR);
    return 0;
}

/* Decide which class (owner/group/others) the current user is in
 * for this file, then check if the requested bit is set.
 * action: 'r' = read, 'w' = write, 'x' = execute/delete */
int perm_check(FileMeta *m, const char *username, const char *usergroup, char action) {
    int shift;
    if (strcmp(username, m->owner) == 0)            shift = 6; /* owner bits  */
    else if (strcmp(usergroup, m->group) == 0)       shift = 3; /* group bits  */
    else                                              shift = 0; /* other bits  */

    int bit = (action == 'r') ? 4 : (action == 'w') ? 2 : 1;
    return (m->mode >> shift) & bit;
}

/*  CRUD operations, gated by perm_check  */

int create_file(const char *filename) {
    if (!require_login()) return -1;
    if (access(filename, F_OK) == 0) { printf("File already exists.\n"); return -1; }

    int fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd < 0) { perror("create_file"); return -1; }
    close(fd);

    FileMeta m;
    strncpy(m.filename, filename, MAX_PATH - 1);
    strncpy(m.owner, current_user, MAX_UNAME - 1);
    strncpy(m.group, current_group, MAX_GROUP - 1);
    m.mode = 0600;      /* owner rw-, nobody else, by default */
    m.encrypted = 0;
    save_file_meta(&m);

    printf("File '%s' created (owner: %s).\n", filename, current_user);
    return 0;
}

int write_to_file(const char *filename, const char *data, int append) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) { printf("No metadata record for '%s'.\n", filename); return -1; }
    if (!perm_check(&m, current_user, current_group, 'w')) { printf("Permission denied: write.\n"); return -1; }
    if (m.encrypted) { printf("File is encrypted — decrypt it first before writing.\n"); return -1; }

    FILE *fp = fopen(filename, append ? "a" : "w");
    if (!fp) { perror("write_to_file"); return -1; }
    fprintf(fp, "%s", data);
    fclose(fp);
    printf("Data written to '%s'.\n", filename);
    return 0;
}

int read_file_contents(const char *filename) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) { printf("No metadata record for '%s'.\n", filename); return -1; }
    if (!perm_check(&m, current_user, current_group, 'r')) { printf("Permission denied: read.\n"); return -1; }
    if (m.encrypted) { printf("File is encrypted — decrypt it first before reading.\n"); return -1; }

    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("read_file_contents"); return -1; }
    char buf[MAX_BUF];
    printf("---- %s ----\n", filename);
    while (fgets(buf, sizeof(buf), fp)) printf("%s", buf);
    printf("\n-------------------\n");
    fclose(fp);
    return 0;
}

int delete_file(const char *filename) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) { printf("No metadata record for '%s'.\n", filename); return -1; }
    if (!perm_check(&m, current_user, current_group, 'x')) { printf("Permission denied: delete.\n"); return -1; }

    if (remove(filename) != 0) { perror("delete_file"); return -1; }
    printf("File '%s' deleted.\n", filename);
    return 0;
}

/* PERMISSION MANAGEMENT */

void view_permissions(const char *filename) {
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) {
        printf("No metadata record for '%s'.\n", filename);
        return;
    }
    printf("File: %s  Owner: %s  Group: %s\n", m.filename, m.owner, m.group);
    printf("Permissions: %c%c%c%c%c%c%c%c%c (%04o)\n",
        (m.mode & 0400) ? 'r' : '-', (m.mode & 0200) ? 'w' : '-', (m.mode & 0100) ? 'x' : '-',
        (m.mode & 0040) ? 'r' : '-', (m.mode & 0020) ? 'w' : '-', (m.mode & 0010) ? 'x' : '-',
        (m.mode & 0004) ? 'r' : '-', (m.mode & 0002) ? 'w' : '-', (m.mode & 0001) ? 'x' : '-',
        m.mode);
}

/* Only the file's owner may change its permissions. */
int set_permissions(const char *filename, const char *mode_str) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) {
        printf("No metadata record for '%s'.\n", filename);
        return -1;
    }
    if (strcmp(current_user, m.owner) != 0) {
        printf("Permission denied: only the owner can change permissions.\n");
        return -1;
    }
    int mode = (int)strtol(mode_str, NULL, 8);
    m.mode = mode;
    save_file_meta(&m);

    /* Mirror onto the real filesystem too, for defense in depth */
    chmod(filename, mode);

    printf("Permissions of '%s' set to %s.\n", filename, mode_str);
    view_permissions(filename);
    return 0;
}

/* ENCRYPTION / DECRYPTION with passphrase verification*/

void keystream_apply(unsigned char *buf, long len, const char *key) {
    int keylen = strlen(key);
    for (long i = 0; i < len; i++)
        buf[i] ^= key[i % keylen];
}

long read_whole_file(const char *path, unsigned char **out_buf) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    *out_buf = malloc(size);
    if (!*out_buf) { fclose(fp); return -1; }
    fread(*out_buf, 1, size, fp);
    fclose(fp);
    return size;
}

int encrypt_file(const char *filename, const char *passphrase) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) { printf("No metadata record for '%s'.\n", filename); return -1; }
    if (strcmp(current_user, m.owner) != 0) { printf("Permission denied: only the owner can encrypt.\n"); return -1; }
    if (m.encrypted) { printf("File is already encrypted.\n"); return -1; }

    unsigned char *plaintext;
    long psize = read_whole_file(filename, &plaintext);
    if (psize < 0) { printf("Could not read file.\n"); return -1; }

    long total = 4 + psize;             /* marker + plaintext */
    unsigned char *buf = malloc(total);
    memcpy(buf, MARKER, 4);
    memcpy(buf + 4, plaintext, psize);
    free(plaintext);

    keystream_apply(buf, total, passphrase);

    FILE *out = fopen(filename, "wb");
    if (!out) { perror("encrypt_file"); free(buf); return -1; }
    fwrite(buf, 1, total, out);
    fclose(out);
    free(buf);

    chmod(filename, S_IRUSR | S_IWUSR); /* owner-only once encrypted */
    m.encrypted = 1;
    save_file_meta(&m);

    printf("File '%s' encrypted successfully.\n", filename);
    return 0;
}

int decrypt_file(const char *filename, const char *passphrase) {
    if (!require_login()) return -1;
    FileMeta m;
    if (get_file_meta(filename, &m) != 0) { printf("No metadata record for '%s'.\n", filename); return -1; }
    if (strcmp(current_user, m.owner) != 0) { printf("Permission denied: only the owner can decrypt.\n"); return -1; }
    if (!m.encrypted) { printf("File is not encrypted.\n"); return -1; }

    unsigned char *buf;
    long total = read_whole_file(filename, &buf);
    if (total < 4) { printf("File is corrupted or too small.\n"); free(buf); return -1; }

    keystream_apply(buf, total, passphrase);

    /* Verify marker BEFORE touching the file on disk */
    if (memcmp(buf, MARKER, 4) != 0) {
        printf("Wrong passphrase — decryption aborted. File left unchanged.\n");
        free(buf);
        return -1;
    }

    FILE *out = fopen(filename, "wb");
    if (!out) { perror("decrypt_file"); free(buf); return -1; }
    fwrite(buf + 4, 1, total - 4, out);   /* strip marker, write plaintext */
    fclose(out);
    free(buf);

    m.encrypted = 0;
    m.mode = 0600;
    save_file_meta(&m);
    chmod(filename, m.mode);

    printf("File '%s' decrypted successfully — passphrase verified.\n", filename);
    return 0;
}

/* CLI INTEGRATION */

void print_unauth_menu(void) {
    printf("\n===== Secure File Management System =====\n");
    printf("1. Register\n2. Login\n0. Exit\nChoice: ");
}

void print_auth_menu(void) {
    printf("\n===== Secure File Management System =====\n");
    printf("Logged in as: %s (group: %s)\n", current_user, current_group);
    printf("1. Create File\n2. Read File\n3. Write File\n4. Delete File\n");
    printf("5. List Files\n6. Change Permissions (chmod)\n");
    printf("7. Encrypt File\n8. Decrypt File\n9. Logout\n0. Exit\nChoice: ");
}

void list_files(void) {
    FILE *fp = fopen(FILE_DB, "r");
    if (!fp) { printf("No files tracked yet.\n"); return; }
    char line[512];
    printf("---- Tracked Files ----\n");
    while (fgets(line, sizeof(line), fp)) {
        FileMeta m; int mode;
        sscanf(line, "%255[^:]:%49[^:]:%31[^:]:%o:%d",
               m.filename, m.owner, m.group, &mode, &m.encrypted);
        printf("%-20s owner=%-10s group=%-8s mode=%04o %s\n",
               m.filename, m.owner, m.group, mode, m.encrypted ? "[ENCRYPTED]" : "");
    }
    fclose(fp);
}

int main() {
    int choice;
    char a[MAX_UNAME], b[MAX_UNAME], path[MAX_PATH], buf[MAX_BUF];

    while (1) {
        if (!logged_in) {
            print_unauth_menu();
            if (scanf("%d", &choice) != 1) break;
            switch (choice) {
                case 1:
                    printf("Username: "); scanf("%s", a);
                    printf("Password: "); scanf("%s", b);
                    register_user(a, b);
                    break;
                case 2:
                    printf("Username: "); scanf("%s", a);
                    printf("Password: "); scanf("%s", b);
                    login_user(a, b);
                    break;
                case 0:
                    printf("Exiting.\n");
                    return 0;
                default:
                    printf("Invalid choice.\n");
            }
        } else {
            print_auth_menu();
            if (scanf("%d", &choice) != 1) break;
            switch (choice) {
                case 1:
                    printf("Filename: "); scanf("%s", path);
                    create_file(path);
                    break;
                case 2:
                    printf("Filename: "); scanf("%s", path);
                    read_file_contents(path);
                    break;
                case 3:
                    printf("Filename: "); scanf("%s", path);
                    printf("Text: "); scanf(" %[^\n]", buf);
                    write_to_file(path, buf, 1);
                    break;
                case 4:
                    printf("Filename: "); scanf("%s", path);
                    delete_file(path);
                    break;
                case 5:
                    list_files();
                    break;
                case 6:
                    printf("Filename: "); scanf("%s", path);
                    printf("Mode (e.g. 640): "); scanf("%s", a);
                    set_permissions(path, a);
                    break;
                case 7:
                    printf("Filename: "); scanf("%s", path);
                    printf("Passphrase: "); scanf("%s", a);
                    encrypt_file(path, a);
                    break;
                case 8:
                    printf("Filename: "); scanf("%s", path);
                    printf("Passphrase: "); scanf("%s", a);
                    decrypt_file(path, a);
                    break;
                case 9:
                    logout_user();
                    break;
                case 0:
                    printf("Exiting.\n");
                    return 0;
                default:
                    printf("Invalid choice.\n");
            }
        }
    }
    return 0;
}
