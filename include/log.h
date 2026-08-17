#ifndef UB_LOG_H
#define UB_LOG_H

#include <windows.h>

BOOL ub_log_init(void);       /* открывает %LOCALAPPDATA%\UnderBuffer\underbuffer.log */
void ub_log(const WCHAR *fmt, ...);
void ub_log_shutdown(void);

#endif /* UB_LOG_H */
