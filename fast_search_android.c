#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>

// Volatile tells the compiler the value may change unexpectedly
volatile bool keep_running = true;

// Signal handler function
void handle_sigint(int sig) {
    printf("\nStopped by Ctrl+C (Signal %d). Cleaning up...\n", sig);
    keep_running = false; // Exit the while loop
}
#define MAX_PATH_LEN 1024
#define MAX_TOKENS 16
#define MAX_TOKEN_LEN 64

// Color definitions for visual highlighting
#define COLOR_RESET "\033[0m"
#define COLOR_PATH  "\033[1;34m"
#define COLOR_MATCH "\033[1;31m"
#define COLOR_INFO  "\033[1;32m"

typedef struct {
    char internal[MAX_PATH_LEN];
    char external[MAX_PATH_LEN];
    int has_external;
} StoragePaths;

typedef struct {
    char tokens[MAX_TOKENS][MAX_TOKEN_LEN];
    int count;
} QueryConfig;

// Global match counter
int total_matches = 0;

// Function declarations
void auto_discover_storage(StoragePaths *paths);
int tokenize_query(const char *query, QueryConfig *q_cfg);
int execute_fzf_v1_match(const char *text, const QueryConfig *q_cfg, char *marker_mask);
void print_highlighted_result(const char *text, const char *marker_mask);
void search_directory_recursive(const char *dir_path, const QueryConfig *q_cfg);

int main(void) {
    signal(SIGINT, handle_sigint);
    while (keep_running) {
    StoragePaths paths = {0};
    auto_discover_storage(&paths);

    printf(COLOR_INFO "--- Termux Autonomous Storage Profile ---\n" COLOR_RESET);
    printf("Internal Path : " COLOR_PATH "%s\n" COLOR_RESET, paths.internal);
    if (paths.has_external) {
        printf("External SDCard: " COLOR_PATH "%s\n" COLOR_RESET, paths.external);
    } else {
        printf("External SDCard: [Not Found or Access Not Configured]\n");
    }
    printf("-----------------------------------------\n\n");

    char query_buffer[256];
    printf("Enter multi-word fuzzy search query: ");
    if (!fgets(query_buffer, sizeof(query_buffer), stdin)) {
        return 0;
    }

    // Strip newline
    query_buffer[strcspn(query_buffer, "\n")] = '\0';

    QueryConfig q_cfg;
    if (!tokenize_query(query_buffer, &q_cfg)) {
        printf("Empty or invalid query string.\n");
        return 0;
    }

    printf("\n" COLOR_INFO "Scanning storage for matches (Press Ctrl+C to abort)..." COLOR_RESET "\n");

    // Recursively scan internal storage
    search_directory_recursive(paths.internal, &q_cfg);

    // Recursively scan external storage if available
    if (paths.has_external) {
        search_directory_recursive(paths.external, &q_cfg);
    }

    printf("\n" COLOR_INFO "Total unique file paths matching query: %d" COLOR_RESET "\n", total_matches);
    //return 0;
    total_matches=0;
    }
}

// Automatically sweeps mounts and standard environments to locate paths natively
void auto_discover_storage(StoragePaths *paths) {
    strncpy(paths->internal, "/storage/emulated/0", MAX_PATH_LEN - 1);
    paths->has_external = 0;

    FILE *mounts = fopen("/proc/mounts", "r");
    if (mounts) {
        char line[512];
        while (fgets(line, sizeof(line), mounts)) {
            char *storage_ptr = strstr(line, "/storage/");
            if (storage_ptr) {
                char candidate[MAX_PATH_LEN];
                if (sscanf(storage_ptr, "%s", candidate) > 0) {
                    char *sub_dir = candidate + 9; // Skip "/storage/"
                    if (strlen(sub_dir) >= 9 && sub_dir[4] == '-') {
                        sub_dir[9] = '\0'; // Truncate to "XXXX-XXXX"
                        snprintf(paths->external, MAX_PATH_LEN, "/storage/%s", sub_dir);
                        paths->has_external = 1;
                        break;
                    }
                }
            }
        }
        fclose(mounts);
    }

    if (!paths->has_external) {
        DIR *dir = opendir("/storage");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strlen(entry->d_name) == 9 && entry->d_name[4] == '-') {
                    snprintf(paths->external, MAX_PATH_LEN, "/storage/%s", entry->d_name);
                    paths->has_external = 1;
                    break;
                }
            }
            closedir(dir);
        }
    }
}

// Tokenizes string query into independent subcomponents
int tokenize_query(const char *query, QueryConfig *q_cfg) {
    q_cfg->count = 0;
    char temp[256];
    strncpy(temp, query, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char *token = strtok(temp, " ");
    while (token != NULL && q_cfg->count < MAX_TOKENS) {
        strncpy(q_cfg->tokens[q_cfg->count], token, MAX_TOKEN_LEN - 1);
        q_cfg->tokens[q_cfg->count][MAX_TOKEN_LEN - 1] = '\0';
        q_cfg->count++;
        token = strtok(NULL, " ");
    }
    return q_cfg->count;
}

// True FZF V1 core algorithm: Case-insensitive, multi-word matching regardless of order
int execute_fzf_v1_match(const char *text, const QueryConfig *q_cfg, char *marker_mask) {
    int text_len = (int)strlen(text);
    memset(marker_mask, 0, text_len);

    char *lower_text = malloc(text_len + 1);
    if (!lower_text) return 0;
    for (int i = 0; i < text_len; i++) {
        lower_text[i] = (char)tolower((unsigned char)text[i]);
    }
    lower_text[text_len] = '\0';

    for (int t = 0; t < q_cfg->count; t++) {
        char lower_tok[MAX_TOKEN_LEN];
        int tok_len = (int)strlen(q_cfg->tokens[t]);
        for (int i = 0; i < tok_len; i++) {
            lower_tok[i] = (char)tolower((unsigned char)q_cfg->tokens[t][i]);
        }
        lower_tok[tok_len] = '\0';

        char *match_ptr = strstr(lower_text, lower_tok);
        if (!match_ptr) {
            free(lower_text);
            return 0; // Token missing entirely
        }

        while (match_ptr) {
            int start_idx = (int)(match_ptr - lower_text);
            for (int m = 0; m < tok_len; m++) {
                marker_mask[start_idx + m] = 1;
            }
            match_ptr = strstr(lower_text + start_idx + 1, lower_tok);
        }
    }

    free(lower_text);
    return 1;
}

// Recursively walks down file trees looking for valid matches
void search_directory_recursive(const char *dir_path, const QueryConfig *q_cfg) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;

    struct dirent *entry;
    char mask[MAX_PATH_LEN];
    char full_path[MAX_PATH_LEN];

    while ((entry = readdir(dir)) != NULL) {
        // Skip relational directory dots
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        // First evaluate the current item against FZF search criteria
        if (execute_fzf_v1_match(full_path, q_cfg, mask)) {
            print_highlighted_result(full_path, mask);
            total_matches++;
        }

        // Drill down if it's a directory
        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
            search_directory_recursive(full_path, q_cfg);
        }
    }
    closedir(dir);
}

// Renders the matched string to stdout using clear inline color boundaries
void print_highlighted_result(const char *text, const char *marker_mask) {
    int len = (int)strlen(text);
    int inside_highlight = 0;

    for (int i = 0; i < len; i++) {
        if (marker_mask[i] && !inside_highlight) {
            printf(COLOR_MATCH);
            inside_highlight = 1;
        } else if (!marker_mask[i] && inside_highlight) {
            printf(COLOR_RESET);
            inside_highlight = 0;
        }
        putchar(text[i]);
    }
    if (inside_highlight) {
        printf(COLOR_RESET);
    }
    printf("\n");
}
