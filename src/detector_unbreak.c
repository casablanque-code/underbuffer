#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <detector.h>

int ub_detect_unbreak(const char *in, char *out, size_t max_len) {
    if (!in || !out || max_len == 0) return 0;

    size_t i = 0;
    size_t out_idx = 0;
    int modified = 0;

    while (in[i] != '\0' && out_idx < max_len - 1) {
        if ((in[i] == '\r' && in[i+1] == '\n') || in[i] == '\n') {
            int step = (in[i] == '\r') ? 2 : 1;

            // 1. Перенос слова через дефис (метри- / ками)
            if (out_idx > 0 && out[out_idx - 1] == '-') {
                out_idx--; // удаляем дефис
                i += step;
                modified = 1;
                continue;
            }

            // 2. Конец предложения (точка, знак вопроса и т.д.) — сохраняем перенос
            if (out_idx > 0 && strchr(".!?:;", out[out_idx - 1])) {
                out[out_idx++] = '\n';
                i += step;
                continue;
            }

            // 3. Обычный разрыв строки посреди предложения — меняем на пробел
            if (out_idx > 0 && out[out_idx - 1] != ' ') {
                out[out_idx++] = ' ';
                modified = 1;
            }
            i += step;
            continue;
        }

        // Удаление двойных пробелов
        if (in[i] == ' ' && out_idx > 0 && out[out_idx - 1] == ' ') {
            i++;
            modified = 1;
            continue;
        }

        out[out_idx++] = in[i++];
    }
    out[out_idx] = '\0';
    return modified;
}
