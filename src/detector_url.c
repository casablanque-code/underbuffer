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
        snprintf(url, 2048, "%s?%s", base, clean_query);
    } else {
        snprintf(url, 2048, "%s", base);
    }
}

int ub_detect_url(const char *in, char *out, size_t max_len) {
    if (!in || !out || max_len == 0) return 0;

    char temp[4096];
    strncpy(temp, in, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    char normalized[4096] = {0};
    size_t n_idx = 0;
    for (size_t i = 0; temp[i] != '\0' && n_idx < sizeof(normalized) - 1; i++) {
        if (strncmp(&temp[i], "https: //", 9) == 0) {
            strcat(normalized, "https://");
            n_idx += 8;
            i += 8;
            continue;
        }
        if (strncmp(&temp[i], "http: //", 8) == 0) {
            strcat(normalized, "http://");
            n_idx += 7;
            i += 7;
            continue;
        }
        normalized[n_idx++] = temp[i];
    }
    normalized[n_idx] = '\0';

    char *url_start = strstr(normalized, "http://");
    if (!url_start) url_start = strstr(normalized, "https://");

    if (url_start) {
        char url_buf[2048];
        size_t len = 0;
        while (url_start[len] != '\0' && !strchr(" \t\r\n)]>\"'", url_start[len])) {
            len++;
        }
        strncpy(url_buf, url_start, len);
        url_buf[len] = '\0';

        clean_single_url(url_buf);

        size_t prefix_len = url_start - normalized;
        snprintf(out, max_len, "%.*s%s%s", (int)prefix_len, normalized, url_buf, url_start + len);
        return (strcmp(in, out) != 0);
    }

    strncpy(out, in, max_len - 1);
    out[max_len - 1] = '\0';
    return 0;
}
