#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_PATH 4096
#define INITIAL_CAPACITY 1024
#define MAX_TOKENS 32
#define RED_TEXT "\033[1;31m"
#define RESET_TEXT "\033[0m"

typedef struct {
    char *start;
    size_t len;
} MatchToken;

char **file_list = NULL;
size_t file_count = 0;
size_t file_capacity = 0;

void handle_sigint(int sig) {
    (void)sig;
    printf("\nExiting scanner.\n");
    for (size_t i = 0; i < file_count; i++) {
        free(file_list[i]);
    }
    free(file_list);
    exit(0);
}

void add_to_list(const char *path) {
    if (file_count >= file_capacity) {
        file_capacity = file_capacity == 0 ? INITIAL_CAPACITY : file_capacity * 2;
        char **new_list = realloc(file_list, file_capacity * sizeof(char *));
        if (!new_list) return;
        file_list = new_list;
    }
    file_list[file_count] = strdup(path);
    if (file_list[file_count]) {
        file_count++;
    }
}

void scan_directory(const char *dir_name) {
    DIR *dir = opendir(dir_name);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char path[MAX_PATH];
        int len = snprintf(path, sizeof(path), "%s/%s", dir_name, entry->d_name);
        if (len >= (int)sizeof(path)) continue;

        add_to_list(path);

        if (entry->d_type == DT_DIR) {
            scan_directory(path);
        } else if (entry->d_type == DT_UNKNOWN) {
            struct stat sb;
            if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                scan_directory(path);
            }
        }
    }
    closedir(dir);
}

// Comparison function to sort match tokens by their appearance order in the string
int compare_tokens(const void *a, const void *b) {
    const MatchToken *ta = (const MatchToken *)a;
    const MatchToken *tb = (const MatchToken *)b;
    if (ta->start < tb->start) return -1;
    if (ta->start > tb->start) return 1;
    return 0;
}

void search_and_print(char *query) {
    size_t match_count = 0;
    char *tokens[MAX_TOKENS];
    int token_count = 0;

    // Tokenize the input query by spaces safely
    char *query_copy = strdup(query);
    char *token = strtok(query_copy, " ");
    while (token != NULL && token_count < MAX_TOKENS) {
        tokens[token_count++] = token;
        token = strtok(NULL, " ");
    }

    if (token_count == 0) {
        free(query_copy);
        return;
    }

    MatchToken current_matches[MAX_TOKENS];

    for (size_t i = 0; i < file_count; i++) {
        char *path = file_list[i];
        bool all_tokens_matched = true;

        // Check if all tokens exist in the path string
        for (int t = 0; t < token_count; t++) {
            char *loc = strstr(path, tokens[t]);
            if (!loc) {
                all_tokens_matched = false;
                break;
            }
            current_matches[t].start = loc;
            current_matches[t].len = strlen(tokens[t]);
        }

        if (all_tokens_matched) {
            match_count++;

            // Sort tokens by their memory address location to print sequentially
            qsort(current_matches, token_count, sizeof(MatchToken), compare_tokens);

            char *current_ptr = path;
            for (int t = 0; t < token_count; t++) {
                // Ensure we don't print overlapping or backward segments
                if (current_matches[t].start >= current_ptr) {
                    // Print plain text before the match
                    printf("%.*s", (int)(current_matches[t].start - current_ptr), current_ptr);
                    // Print highlighted match keyword
                    printf("%s%.*s%s", RED_TEXT, (int)current_matches[t].len, current_matches[t].start, RESET_TEXT);
                    current_ptr = current_matches[t].start + current_matches[t].len;
                }
            }
            // Print the rest of the file path
            printf("%s\n", current_ptr);
        }
    }
    printf("\nTotal Matches Found: %zu\n", match_count);
    free(query_copy);
}

int main(void) {
    signal(SIGINT, handle_sigint);

    printf("Scanning entire file system... Please wait.\n");
    scan_directory("/");
    printf("Scan complete. Loaded %zu paths.\n\n", file_count);

    char *input_buffer = NULL;
    size_t buffer_size = 0;

    while (1) {
        printf("fzf-search> ");
        fflush(stdout);

        if (getline(&input_buffer, &buffer_size, stdin) == -1) {
            break;
        }

        input_buffer[strcspn(input_buffer, "\n")] = '\0';

        search_and_print(input_buffer);
        printf("----------------------------------------\n");
    }

    free(input_buffer);
    handle_sigint(0);
    return 0;
}
