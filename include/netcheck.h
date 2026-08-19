#ifndef UB_NETCHECK_H
#define UB_NETCHECK_H

#include <windows.h>

/* WinHTTP error code for "hostname doesn't resolve" -- ERROR_WINHTTP_
 * NAME_NOT_RESOLVED, defined in winhttp.h on the real build. Redefined
 * here as a plain constant so this header (and the pure decision
 * function below) never needs to include winhttp.h -- keeps it
 * testable from tests/compat/windows.h without pulling in real
 * WinHTTP declarations. */
#define UB_ERROR_WINHTTP_NAME_NOT_RESOLVED 12007

/* Fire-and-forget HEAD check on a worker thread, never call from
 * WM_CLIPBOARDUPDATE directly -- this returns immediately.
 *
 * If the cleaned link turns out to be broken, the clipboard is
 * reverted to original_text -- some links only work with the
 * tracking params we stripped attached. "Broken" means either:
 *   - a definitive HTTP 4xx/5xx after following redirects, or
 *   - the hostname doesn't resolve at all (DNS failure) -- unlike a
 *     timeout or connection error, a specific host not existing in
 *     DNS is rarely transient and is strong evidence the cleaned URL
 *     itself is wrong.
 * Any other failed *request* (timeout, connection refused, TLS
 * error, ...) does NOT trigger a revert -- too ambiguous to justify
 * undoing the user's clipboard over what could just be a flaky
 * network.
 *
 * The revert only happens if the clipboard sequence still equals
 * seq_after_write (nothing else has copied anything since our
 * cleanup write). */
void ub_netcheck_start_async(HWND owner, const WCHAR *cleaned_url,
                              const WCHAR *original_text,
                              DWORD seq_after_write, DWORD timeout_ms);

/* Pure decision extracted for unit testing without a real network
 * call. request_succeeded=FALSE means no HTTP response was obtained
 * at all; last_error is whatever GetLastError() returned in that
 * case (ignored when request_succeeded is TRUE). */
static inline BOOL ub_netcheck_should_revert(BOOL request_succeeded, DWORD http_status,
                                               DWORD last_error)
{
    if (request_succeeded) return http_status >= 400;
    return last_error == UB_ERROR_WINHTTP_NAME_NOT_RESOLVED;
}

#endif
