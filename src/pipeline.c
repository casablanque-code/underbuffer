#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/*
 * ВАЖНО: детекторы -- WCHAR* -> WCHAR*, без промежуточной конвертации
 * в UTF-8 и обратно (см. комментарий в detector_url.c про то, почему
 * от char*+UTF-8 отказались -- это ломало вызов ub_detect_json через
 * несовместимый прототип и не давало собрать нативные тесты).
 *
 * Base64/URL-decode детектор в MVP не включаем: авто-декодинг
 * произвольных строк слишком легко ломает то, что скопировано
 * намеренно "как есть" (токены, пароли, ключи).
 */
WCHAR *ub_detect_url(const WCHAR *input);
WCHAR *ub_detect_json(const WCHAR *input);
WCHAR *ub_detect_unbreak(const WCHAR *input);

const ub_detector_t UB_SYNC_PIPELINE[] = {
    { "unbreak", ub_detect_unbreak }, /* сначала склеиваем рваные строки */
    { "json",    ub_detect_json    }, /* затем пробуем распознать JSON   */
    { "url",     ub_detect_url     }, /* и только затем -- чистка URL    */
};
const size_t UB_SYNC_PIPELINE_COUNT = sizeof(UB_SYNC_PIPELINE) / sizeof(UB_SYNC_PIPELINE[0]);

WCHAR *ub_pipeline_run_sync(const WCHAR *input)
{
    WCHAR *current = _wcsdup(input);
    if (!current) return NULL;

    for (size_t i = 0; i < UB_SYNC_PIPELINE_COUNT; i++) {
        WCHAR *next = UB_SYNC_PIPELINE[i].run(current);
        if (next != NULL) {
            free(current);
            current = next;
        }
    }
    return current;
}
