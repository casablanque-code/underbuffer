#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <detector.h>

// Явные прототипы детекторов на случай отсутствия их в detector.h
int ub_detect_unbreak(const char *in, char *out, size_t max_len);
int ub_detect_url(const char *in, char *out, size_t max_len);
int ub_detect_json(const char *in, char *out, size_t max_len);

WCHAR *ub_pipeline_run_sync(const WCHAR *input) {
    if (!input) return NULL;

    int utf8_len = WideCharToMultiByte(CP_UTF8, 0, input, -1, NULL, 0, NULL, NULL);
    if (utf8_len <= 0) return NULL;

    size_t buf_size = (size_t)utf8_len * 4 + 4096;
    char *buf_a = (char *)malloc(buf_size);
    char *buf_b = (char *)malloc(buf_size);
    if (!buf_a || !buf_b) {
        free(buf_a);
        free(buf_b);
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, input, -1, buf_a, (int)buf_size, NULL, NULL);

    // 1. Unbreak (склейка переносов)
    if (ub_detect_unbreak(buf_a, buf_b, buf_size)) {
        strcpy(buf_a, buf_b);
    }

    // 2. URL Cleaning (очистка ссылок)
    if (ub_detect_url(buf_a, buf_b, buf_size)) {
        strcpy(buf_a, buf_b);
    }

    // 3. JSON Formatting
    if (ub_detect_json(buf_a, buf_b, buf_size)) {
        strcpy(buf_a, buf_b);
    }

    int wide_len = MultiByteToWideChar(CP_UTF8, 0, buf_a, -1, NULL, 0);
    WCHAR *result = (WCHAR *)malloc(wide_len * sizeof(WCHAR));
    if (result) {
        MultiByteToWideChar(CP_UTF8, 0, buf_a, -1, result, wide_len);
    }

    free(buf_a);
    free(buf_b);

    return result;
}
