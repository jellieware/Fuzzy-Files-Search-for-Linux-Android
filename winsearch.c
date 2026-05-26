#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>

#define MAX_INPUT 256
#define MAX_FILES 1500000
#define MAX_PATH_LEN 260
#define MAX_TOKENS 16
#define SCORE_MATCH_START 100
#define SCORE_MATCH_REGULAR 10

typedef struct {
    char path[MAX_PATH_LEN];
    int score;
} FileItem;

// Global thread-safe state
FileItem *g_files = NULL;
int g_file_count = 0;
BOOL g_scanning_done = FALSE;
CRITICAL_SECTION g_data_cs;

void set_color(WORD color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

// Case-insensitive validation that an exact, unbroken substring exists
// Returns the index where the match starts, or -1 if it does not exist
int find_strict_substring(const char *text, const char *token) {
    int t_len = (int)strlen(text);
    int tok_len = (int)strlen(token);
    if (tok_len == 0) return -1;

    for (int i = 0; i <= t_len - tok_len; i++) {
        int match = 1;
        for (int j = 0; j < tok_len; j++) {
            if (tolower(text[i + j]) != tolower(token[j])) {
                match = 0;
                break; // Letters are separated or mismatched; abort this position
            }
        }
        if (match) {
            return i; // Found an unbroken, side-by-side match sequence
        }
    }
    return -1;
}

// Evaluates all input words. Every single word must be a strict, unbroken substring.
int evaluate_multi_substring(const char *text, char tokens[MAX_TOKENS][MAX_INPUT], int token_count, int *final_map) {
    if (token_count == 0) return 0;
    
    memset(final_map, 0, MAX_PATH_LEN * sizeof(int));
    int total_score = 0;
    int path_len = (int)strlen(text);

    for (int i = 0; i < token_count; i++) {
        int start_idx = find_strict_substring(text, tokens[i]);
        if (start_idx == -1) {
            return -100000; // Discard completely if any word has separated letters
        }
        
        int token_len = (int)strlen(tokens[i]);
        
        // Map out the precise, unbroken block for red highlighting
        for (int j = 0; j < token_len; j++) {
            final_map[start_idx + j] = 1;
        }
        
        int word_score = SCORE_MATCH_REGULAR;
        if (start_idx == 0 || text[start_idx - 1] == '\\' || text[start_idx - 1] == '/' || 
            text[start_idx - 1] == '_'  || text[start_idx - 1] == '-' || text[start_idx - 1] == ' ') {
            word_score += SCORE_MATCH_START; // Word boundary bonus
        }
        
        total_score += word_score;
    }
    
    total_score -= path_len; // Shorter path tie-breaker
    return total_score;
}

// Background thread file crawler
void scan_directory(const char *dir_path) {
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, sizeof(search_path), "%s\\*", dir_path);

    WIN32_FIND_DATAA find_data;
    HANDLE h_find = FindFirstFileA(search_path, &find_data);
    if (h_find == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
            continue;

        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s\\%s", dir_path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)) {
                scan_directory(full_path);
            }
        } else {
            EnterCriticalSection(&g_data_cs);
            if (g_file_count < MAX_FILES) {
                strncpy(g_files[g_file_count].path, full_path, MAX_PATH_LEN);
                g_files[g_file_count].score = 0;
                g_file_count++;
            }
            LeaveCriticalSection(&g_data_cs);
        }
    } while (FindNextFileA(h_find, &find_data));

    FindClose(h_find);
}

DWORD WINAPI drive_scanner_thread(LPVOID param) {
    scan_directory("C:\\");
    g_scanning_done = TRUE;
    return 0;
}

int compare_items(const void *a, const void *b) {
    return ((FileItem *)b)->score - ((FileItem *)a)->score;
}

void clear_screen() {
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hCon, &csbi);
    FillConsoleOutputCharacter(hCon, (TCHAR)' ', csbi.dwSize.X * csbi.dwSize.Y, coordScreen, &cCharsWritten);
    SetConsoleCursorPosition(hCon, coordScreen);
}

int main() {
    g_files = malloc(sizeof(FileItem) * MAX_FILES);
    InitializeCriticalSection(&g_data_cs);

    HANDLE h_thread = CreateThread(NULL, 0, drive_scanner_thread, NULL, 0, NULL);
    if (h_thread) CloseHandle(h_thread);

    char input_buf[MAX_INPUT] = {0};
    int input_len = 0;

    // Interactive UI loop
    while (1) {
        clear_screen();
        
        // Parse raw string into individual token words
        char tokens[MAX_TOKENS][MAX_INPUT];
        int token_count = 0;
        char temp_buf[MAX_INPUT];
        strncpy(temp_buf, input_buf, MAX_INPUT);
        
        char *tok = strtok(temp_buf, " ");
        while (tok && token_count < MAX_TOKENS) {
            strncpy(tokens[token_count++], tok, MAX_INPUT);
            tok = strtok(NULL, " ");
        }
        
        int matched_count = 0;
        FileItem *filtered = malloc(sizeof(FileItem) * MAX_FILES);

        EnterCriticalSection(&g_data_cs);
        for (int i = 0; i < g_file_count; i++) {
            if (input_len == 0) {
                filtered[matched_count] = g_files[i];
                filtered[matched_count].score = 0;
                matched_count++;
            } else {
                int dummy_map[MAX_PATH_LEN];
                int score = evaluate_multi_substring(g_files[i].path, tokens, token_count, dummy_map);
                if (score > -50000) {
                    filtered[matched_count] = g_files[i];
                    filtered[matched_count].score = score;
                    matched_count++;
                }
            }
        }
        int total_scanned = g_file_count;
        LeaveCriticalSection(&g_data_cs);

        if (input_len > 0 && matched_count > 0) {
            qsort(filtered, matched_count, sizeof(FileItem), compare_items);
        }

        // Render Panel Headers
        printf("> Search: %s_\n", input_buf);
        printf("[%d/%d files crawled] -- Found %d results\n", total_scanned, MAX_FILES, matched_count);
        printf("--------------------------------------------------------------------------\n");

        // UI Print Loop (Top 20 Entries)
        int display_limit = (matched_count > 20) ? 20 : matched_count;
        for (int i = 0; i < display_limit; i++) {
            int map[MAX_PATH_LEN] = {0};
            if (input_len > 0) {
                evaluate_multi_substring(filtered[i].path, tokens, token_count, map);
            }

            printf("  ");
            for (int j = 0; filtered[i].path[j] != '\0'; j++) {
                if (map[j]) {
                    set_color(FOREGROUND_RED | FOREGROUND_INTENSITY); // Bold Red contiguous matches
                } else {
                    set_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE); // Default standard color
                }
                putchar(filtered[i].path[j]);
            }
            set_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            printf("\n");
        }

        free(filtered);

        // Async non-blocking character input listener
        while (!_kbhit()) {
            if (!g_scanning_done) {
                Sleep(80); 
                goto refresh_ui;
            }
            Sleep(10);
        }

        int ch = _getch();
        if (ch == 27) { // Escape Key
            break;
        } else if (ch == 8) { // Backspace
            if (input_len > 0) {
                input_buf[--input_len] = '\0';
            }
        } else if (ch >= 32 && ch <= 126 && input_len < MAX_INPUT - 1) {
            input_buf[input_len++] = (char)ch;
            input_buf[input_len] = '\0';
        }

    refresh_ui:;
    }

    DeleteCriticalSection(&g_data_cs);
    free(g_files);
    clear_screen();
    return 0;
}
