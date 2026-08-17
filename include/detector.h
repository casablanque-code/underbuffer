#ifndef UB_DETECTOR_H
#define UB_DETECTOR_H

#include <windows.h>

/*
 * Каждый детектор — чистая функция: вход WCHAR*, выход malloc'нутый
 * WCHAR* (новый буфер) или NULL, если детектор не применим и текст
 * менять не нужно (тогда пайплайн просто идёт к следующему детектору
 * с прежним текстом). Детекторы НЕ трогают буфер обмена напрямую —
 * это обязанность clipboard.c. Такое разделение и делает пайплайн
 * тестируемым и безопасно расширяемым.
 */
typedef WCHAR *(*ub_detector_fn)(const WCHAR *input);

typedef struct {
    const char *name;
    ub_detector_fn run;
} ub_detector_t;

/* Синхронные детекторы MVP: UTM/tracker-стрипинг + нормализация URL,
 * pretty-print JSON, склейка рваных строк. Порядок в массиве и есть
 * порядок применения. */
extern const ub_detector_t UB_SYNC_PIPELINE[];
extern const size_t UB_SYNC_PIPELINE_COUNT;

/* Прогоняет input через весь синхронный пайплайн. Возвращает
 * malloc'нутый результат (даже если ни один детектор не сработал —
 * в этом случае это просто копия input, чтобы вызывающему коду не
 * пришлось различать "изменено/не изменено"). NULL при OOM. */
WCHAR *ub_pipeline_run_sync(const WCHAR *input);

#endif /* UB_DETECTOR_H */
