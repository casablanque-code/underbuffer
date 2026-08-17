#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <detector.h>

static int is_tracker_param(const char *param) {
    if (strncmp(param, "utm_", 4) == 0) return 1;
    if (strncmp(param, "si=", 3) == 0) return 1;
    if (strncmp(param, "fbclid=", 7) == 0) return 1;
    if (strncmp(param, "gclid=", 6) == 0) return 1;
    return 0;
}

static void clean_single_url(char *url) {
    char *q = strchr(url, '?');
    if (!q) return;

    char base[2048];
    size_t base_len = q - url;
    if (base_len >= sizeof(base)) return;
    strncpy(base, url, base_len);
    base[base_len] = '\0';

    char *hash_mark = strchr(q, '#');
    char fragment[512] = {0};
    if (hash_mark) {
        snprintf(fragment, sizeof(fragment), "%s", hash_mark);
        *hash_mark = '\0';
    }

    char query[2048];
    snprintf(query, sizeof(query), "%s", q + 1);

    char clean_query[2048] = {0};
    char *token = strtok(query, "&");
    while (token) {
        if (!is_tracker_param(token)) {
            if (clean_query[0] != '\0') strcat(clean_query, "&");
            strcat(clean_query, token);
        }
        token = strtok(NULL, "&");
    }

    if (clean_query[0] != '\0') {
        snprintf(url, 2048, "%s?%s%s", base, clean_query, fragment);
    } else {
        snprintf(url, 2048, "%s%s", base, fragment);
    }
}

int ub_detect_url(const char *in, char *out, size_t max_len) {
    if (!in || !out || max_len == 0) return 0;

    char current[4096];
    strncpy(current, in, sizeof(current) - 1);
    current[sizeof(current) - 1] = '\0';

    int modified = 0;

    while (1) {
        char *url_start = strstr(current, "http://");
        if (!url_start) url_start = strstr(current, "https://");
        if (!url_start) break;

        // Ищем конец URL
        size_t len = 0;
        while (url_start[len] != '\0' && !strchr(" \t\r\n)]>\"'", url_start[len])) {
            len++;
        }

        char url_buf[2048];
        if (len >= sizeof(url_buf)) break;
        strncpy(url_buf, url_start, len);
        url_buf[len] = '\0';

        char orig_url[2048];
        strcpy(orig_url, url_buf);

        clean_single_url(url_buf);

        if (strcmp(orig_url, url_buf) != 0) {
            char next_buf[4096];
            size_t prefix_len = url_start - current;
            snprintf(next_buf, sizeof(next_buf), "%.*s%s%s", (int)prefix_len, current, url_buf, url_start + len);
            strncpy(current, next_buf, sizeof(current) - 1);
            current[sizeof(current) - 1] = '\0';
            modified = 1;
        } else {
            // Если URL не изменился, ищем следующий за ним
            char *next_search = url_start + len;
            if (*next_search == '\0') break;
            // Временно заменяем обработанную схему, чтобы strstr не зацикливался
            url_start[0] = 'H'; 
        }
    }

    // Возвращаем 'http' / 'https' на место, если меняли временно
    for (size_t i = 0; current[i] != '\0'; i++) {
        if (strncmp(&current[i], "Http://", 7) == 0) current[i] = 'h';
        if (strncmp(&current[i], "Https://", 8) == 0) current[i] = 'h';
    }

    strncpy(out, current, max_len - 1);
    out[max_len - 1] = '\0';
    return modified;
}
