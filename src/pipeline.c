#include "detector.h"
#include <stdlib.h>
#include <wchar.h>

/* Реализации живут в detector_url.c / detector_json.c / detector_unbreak.c.
 * Base64/URL-decode детектор сознательно исключён из MVP: авто-декодинг
 * произвольных base64-подобных строк слишком легко ломает то, что
 * пользователь скопировал намеренно "как есть" (токены, пароли, ключи).
 * Если понадобится — добавляется отдельным opt-in детектором, никогда
 * не в состав пайплайна по умолчанию. */
WCHAR *ub_detect_url(const WCHAR *input);
WCHAR *ub_detect_json(const WCHAR *input);
WCHAR *ub_detect_unbreak(const WCHAR *input);

const ub_detector_t UB_SYNC_PIPELINE[] = {
    { "unbreak", ub_detect_unbreak }, /* сначала склеиваем рваные строки */
    { "json",    ub_detect_json    }, /* затем пробуем распознать JSON   */
    { "url",     ub_detect_url     }, /* и только затем — чистка URL     */
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
        /* next == NULL: детектор не применился, current остаётся как есть */
    }
    return current;
}
