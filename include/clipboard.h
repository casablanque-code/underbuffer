#ifndef UB_CLIPBOARD_H
#define UB_CLIPBOARD_H

#include <windows.h>

/*
 * Вся работа с буфером обмена изолирована здесь. Два инварианта,
 * которые обязаны соблюдаться модулем:
 *
 * 1) Anti-recursion: ub_clipboard_write() сама выставляет
 *    g_is_internal_update перед записью и снимает после — вызывающий
 *    код НЕ должен трогать этот флаг руками.
 *
 * 2) Anti-staleness: перед записью результата обработки текста,
 *    полученного асинхронно (например после HEAD-проверки URL),
 *    вызывающий код обязан передать ту же sequence-number, что была
 *    на момент чтения. Если она изменилась — значит пользователь уже
 *    скопировал что-то новое, и запись отменяется.
 */

extern volatile LONG g_is_internal_update;

/* Читает текст буфера как UTF-16 (CF_UNICODETEXT). Возвращает
 * malloc'нутую строку (вызывающий обязан free()) или NULL, если
 * в буфере не текст / буфер занят / текст пуст. *out_seq заполняется
 * текущим GetClipboardSequenceNumber(). */
WCHAR *ub_clipboard_read_text(DWORD *out_seq);

/* Текущий номер последовательности буфера — дешёвый вызов без
 * открытия буфера, безопасен для частого polling/сверки. */
DWORD ub_clipboard_sequence(void);

/* Записывает text обратно в буфер, но только если текущая
 * sequence number равна expected_seq (защита от гонки с другим
 * приложением). Возвращает TRUE при успешной записи, FALSE если
 * буфер устарел или запись не удалась. */
BOOL ub_clipboard_write_if_fresh(HWND owner, const WCHAR *text, DWORD expected_seq);

#endif /* UB_CLIPBOARD_H */
