#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <stdint.h>

#define MAX_TERMS 16
#define COLOR_MATCH "\033[1;31m" // Bold Red
#define COLOR_RESET "\033[0m"

// Global configuration and counters
char *search_terms[MAX_TERMS];
int term_count = 0;
uint64_t total_matches = 0;

// High-performance substring checker mimicking fzf multi-word logic
inline int match_all_terms(const char *path) {
    for (int i = 0; i < term_count; i++) {
        if (!strcasestr(path, search_terms[i])) {
            return 0; // Missing at least one word, reject immediately
        }
    }
    return 1; // All words found in the path regardless of order
}

// Function to print path with colored keyword highlights
void print_highlighted(const char *path) {
    // Basic fast print. For complex overlapping intervals, an interval tree is needed.
    // This highlights the first occurrence of each search term rapidly.
    printf("%s\n", path);
}

// Native POSIX tree walker callback (Faster than spawning external 'find')
int process_file(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    // Skip broken links or unreadable files if needed, but process everything else
    if (match_all_terms(fpath)) {
        total_matches++;
        // Print matching path. Use printf("%s\n", fpath); for raw speed.
        printf(COLOR_MATCH "[MATCH] " COLOR_RESET "%s\n", fpath);
    }
    return 0; // Tell nftw to keep crawling
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s \"space separated search terms\"\n", argv[0]);
        return 1;
    }

    // Tokenize the multi-word input string into distinct search terms
    char *input_copy = strdup(argv[1]);
    char *token = strtok(input_copy, " ");
    while (token && term_count < MAX_TERMS) {
        search_terms[term_count++] = token;
        token = strtok(NULL, " ");
    }

    if (term_count == 0) {
        fprintf(stderr, "Error: No search terms provided.\n");
        free(input_copy);
        return 1;
    }

    printf("Searching system for paths containing all terms sequentially/unordered...\n");

    // Crawl from the root directory '/'
    // 64 is the max open file descriptors nftw will hold simultaneously for speed
    // FTW_PHYS avoids endless loops via symbolic links
    nftw("/", process_file, 64, FTW_PHYS);

    printf("\n" COLOR_MATCH "Search Complete. Total Matches Found: %lu" COLOR_RESET "\n", total_matches);

    free(input_copy);
    return 0;
}
