#include "netcheck.h"
#include "clipboard.h"
#include "log.h"
#include <winhttp.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib, "winhttp.lib")

/* WinHTTP, not WinINet: WinINet targets UI apps with a message loop
 * (auth dialogs, session-context dependence); WinHTTP is Microsoft's
 * recommended choice for services/daemons without a visible UI. */

typedef struct {
    HWND owner;
    WCHAR *cleaned_url;
    WCHAR *original_text;
    DWORD seq_after_write;
    DWORD timeout_ms;
} netcheck_ctx_t;

static DWORD WINAPI netcheck_thread(LPVOID param)
{
    netcheck_ctx_t *ctx = (netcheck_ctx_t *)param;
    BOOL succeeded = FALSE;
    DWORD status = 0;
    DWORD last_error = 0;

    URL_COMPONENTS uc = { 0 };
    uc.dwStructSize = sizeof(uc);
    WCHAR host[256] = { 0 };
    WCHAR path[2048] = { 0 };
    uc.lpszHostName = host;
    uc.dwHostNameLength = ARRAYSIZE(host);
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = ARRAYSIZE(path);

    if (!WinHttpCrackUrl(ctx->cleaned_url, 0, 0, &uc)) {
        last_error = GetLastError();
        ub_log(L"netcheck: WinHttpCrackUrl failed for %s (err=%lu)", ctx->cleaned_url, last_error);
        goto decide;
    }

    HINTERNET hSession = WinHttpOpen(L"UnderBuffer/0.2",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        last_error = GetLastError();
        ub_log(L"netcheck: WinHttpOpen failed (err=%lu)", last_error);
        goto decide;
    }

    WinHttpSetTimeouts(hSession, (int)ctx->timeout_ms, (int)ctx->timeout_ms,
                        (int)ctx->timeout_ms, (int)ctx->timeout_ms);

    HINTERNET hConnect = WinHttpConnect(hSession, uc.lpszHostName, uc.nPort, 0);
    if (!hConnect) {
        last_error = GetLastError();
        ub_log(L"netcheck: WinHttpConnect failed for %s (err=%lu)", ctx->cleaned_url, last_error);
        WinHttpCloseHandle(hSession);
        goto decide;
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"HEAD", uc.lpszUrlPath,
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        last_error = GetLastError();
        ub_log(L"netcheck: WinHttpOpenRequest failed for %s (err=%lu)", ctx->cleaned_url, last_error);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        goto decide;
    }

    BOOL sent = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (sent && WinHttpReceiveResponse(hRequest, NULL)) {
        DWORD status_len = sizeof(status);
        if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_FLAG_NUMBER | WINHTTP_QUERY_STATUS_CODE,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_len,
                                 WINHTTP_NO_HEADER_INDEX)) {
            succeeded = TRUE;
        } else {
            last_error = GetLastError();
        }
    } else {
        last_error = GetLastError();
        ub_log(L"netcheck: request failed for %s (err=%lu)", ctx->cleaned_url, last_error);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

decide:
    if (ub_netcheck_should_revert(succeeded, status, last_error)) {
        BOOL reverted = ub_clipboard_write_if_fresh(ctx->owner, ctx->original_text, ctx->seq_after_write);
        const WCHAR *reason = succeeded ? L"HTTP error" : L"host does not resolve";
        if (reverted) {
            ub_log(L"netcheck: %s -> %s (status=%lu err=%lu), reverted clipboard to original "
                    L"(tracked params may be required)",
                    ctx->cleaned_url, reason, status, last_error);
        } else {
            ub_log(L"netcheck: %s -> %s, wanted to revert but clipboard changed since (stale seq)",
                    ctx->cleaned_url, reason);
        }
    } else if (succeeded) {
        ub_log(L"netcheck: %s -> HTTP %lu, ok", ctx->cleaned_url, status);
    } else {
        ub_log(L"netcheck: %s -> request failed (err=%lu), link status unknown, keeping cleaned version",
                ctx->cleaned_url, last_error);
    }

    free(ctx->cleaned_url);
    free(ctx->original_text);
    free(ctx);
    return 0;
}

void ub_netcheck_start_async(HWND owner, const WCHAR *cleaned_url,
                              const WCHAR *original_text,
                              DWORD seq_after_write, DWORD timeout_ms)
{
    netcheck_ctx_t *ctx = (netcheck_ctx_t *)malloc(sizeof(netcheck_ctx_t));
    if (!ctx) return;

    ctx->owner = owner;
    ctx->cleaned_url = _wcsdup(cleaned_url);
    ctx->original_text = _wcsdup(original_text);
    ctx->seq_after_write = seq_after_write;
    ctx->timeout_ms = timeout_ms;

    if (!ctx->cleaned_url || !ctx->original_text) {
        free(ctx->cleaned_url);
        free(ctx->original_text);
        free(ctx);
        return;
    }

    HANDLE h = CreateThread(NULL, 0, netcheck_thread, ctx, 0, NULL);
    if (h) {
        CloseHandle(h);
    } else {
        free(ctx->cleaned_url);
        free(ctx->original_text);
        free(ctx);
    }
}
