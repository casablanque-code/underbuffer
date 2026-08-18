/*
 * Unit tests for detector_url / detector_json / detector_unbreak / pipeline.
 *
 * Built and run NATIVELY (gcc, no mingw, no Windows) via
 * tests/compat/windows.h — see the comment there. Fast loop: edit
 * src/detector_*.c -> `make test` -> see results in seconds, no exe
 * rebuild and no manual copy-paste into Windows.
 *
 * Final validation before a release still has to go through the usual
 * `make` in WSL + a manual run of the exe on Windows (these tests don't
 * touch clipboard.c/main.c/netcheck.c — the actual Win32 integration).
 */
#include "detector.h"
#include "config.h"
#include "netcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

WCHAR *ub_detect_url(const WCHAR *input);
WCHAR *ub_detect_json(const WCHAR *input);
WCHAR *ub_detect_unbreak(const WCHAR *input);

static int g_failed = 0;
static int g_total = 0;

static void check_eq(const char *test_name, const WCHAR *got, const WCHAR *expected)
{
    g_total++;
    if (got == NULL) {
        printf("[FAIL] %s: detector returned NULL (expected a change)\n", test_name);
        printf("        expected: %ls\n", expected);
        g_failed++;
        return;
    }
    if (wcscmp(got, expected) != 0) {
        printf("[FAIL] %s\n", test_name);
        printf("        expected: %ls\n", expected);
        printf("        got:      %ls\n", got);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

static void check_null(const char *test_name, const WCHAR *got)
{
    g_total++;
    if (got != NULL) {
        printf("[FAIL] %s: expected NULL (detector should not apply), got: %ls\n",
               test_name, got);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

static void check_true(const char *test_name, BOOL got)
{
    g_total++;
    if (!got) {
        printf("[FAIL] %s: expected TRUE, got FALSE\n", test_name);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

static void check_false(const char *test_name, BOOL got)
{
    g_total++;
    if (got) {
        printf("[FAIL] %s: expected FALSE, got TRUE\n", test_name);
        g_failed++;
        return;
    }
    printf("[ OK ] %s\n", test_name);
}

/* ---- detector_url ---- */

static void test_url_bare_strips_trackers(void)
{
    WCHAR *r = ub_detect_url(
        L"https://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz");
    check_eq("url: bare link, trackers stripped",
             r, L"https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    free(r);
}

static void test_url_embedded_in_text(void)
{
    WCHAR *r = ub_detect_url(
        L"Check this out: https://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz -- fire");
    check_eq("url: link embedded in arbitrary text also gets cleaned",
             r,
             L"Check this out: https://www.youtube.com/watch?v=dQw4w9WgXcQ -- fire");
    free(r);
}

static void test_url_markdown_wrapped(void)
{
    WCHAR *r = ub_detect_url(
        L"[link](https://example.com/page?id=1&utm_source=x&utm_medium=y)");
    check_eq("url: markdown wrapper preserved, only the query gets cleaned",
             r, L"[link](https://example.com/page?id=1)");
    free(r);
}

static void test_url_multiple_in_text(void)
{
    WCHAR *r = ub_detect_url(
        L"a https://a.com/?utm_source=x b https://b.com/?fbclid=y c");
    check_eq("url: multiple links in one text -- both get cleaned",
             r, L"a https://a.com/ b https://b.com/ c");
    free(r);
}

static void test_url_no_query_untouched(void)
{
    WCHAR *r = ub_detect_url(L"see https://example.com/page here");
    check_null("url: no query string -- detector does not apply", r);
}

static void test_url_no_url_untouched(void)
{
    WCHAR *r = ub_detect_url(L"just plain text with no links at all");
    check_null("url: text with no URL -- detector does not apply", r);
}

static void test_url_keeps_non_tracker_params(void)
{
    WCHAR *r = ub_detect_url(L"https://example.com/search?q=test&utm_source=x&page=2");
    check_eq("url: useful params kept, trackers stripped",
             r, L"https://example.com/search?q=test&page=2");
    free(r);
}

/* ---- detector_json ---- */

static void test_json_pretty_prints(void)
{
    WCHAR *r = ub_detect_json(L"{\"status\":\"ok\",\"code\":200,\"data\":[\"test1\",\"test2\"]}");
    check_eq("json: compact json gets pretty-printed",
             r,
             L"{\n  \"status\": \"ok\",\n  \"code\": 200,\n  \"data\": [\n    \"test1\",\n    \"test2\"\n  ]\n}");
    free(r);
}

static void test_json_rejects_plain_text(void)
{
    WCHAR *r = ub_detect_json(L"this isn't json, just {some text} with braces");
    check_null("json: non-json (doesn't start with { or [) is left alone", r);
}

/* ---- detector_unbreak ---- */

static void test_unbreak_single_newline_glues_with_space(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\nword");
    check_eq("unbreak: single \\n between words -> space", r, L"word word");
    free(r);
}

static void test_unbreak_double_newline_preserved(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\n\nword\nword");
    check_eq("unbreak: double \\n (paragraph boundary) kept, single \\n glued",
             r, L"word\n\nword word");
    free(r);
}

static void test_unbreak_hyphen_wordbreak(void)
{
    WCHAR *r = ub_detect_unbreak(L"exam-\nple");
    check_eq("unbreak: hyphenated word wrap gets glued without the hyphen",
             r, L"example");
    free(r);
}

static void test_unbreak_list_marker_not_glued(void)
{
    /* A list marker after \n is the only potential change here, so the
     * detector correctly returns NULL (nothing to do). */
    WCHAR *r = ub_detect_unbreak(L"item1\n- item2");
    check_null("unbreak: list marker after \\n is not glued (no change -> NULL)", r);
}

static void test_unbreak_list_marker_not_glued_with_other_change(void)
{
    /* Same list marker, but there's also a regular break nearby that
     * should get glued -- the list marker itself stays untouched. */
    WCHAR *r = ub_detect_unbreak(L"this\nwas\n- item");
    check_eq("unbreak: list marker stays put, neighboring break gets glued",
             r, L"this was\n- item");
    free(r);
}

static void test_unbreak_space_before_newline_regression(void)
{
    /* Regression: "word \nword" used to leave BOTH the space AND the
     * raw \n, because the "is the char before \n a letter?" check
     * looked directly at the space that was already written instead
     * of looking through it. */
    WCHAR *r = ub_detect_unbreak(L"word \nword");
    check_eq("unbreak: a space before \\n no longer lets the \\n leak through",
             r, L"word word");
    free(r);
}

static void test_unbreak_multi_spaces_collapse(void)
{
    WCHAR *r = ub_detect_unbreak(L"word   word\nword");
    check_eq("unbreak: runs of spaces collapse into one",
             r, L"word word word");
    free(r);
}

static void test_unbreak_no_single_newline_untouched(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\n\nword");
    check_null("unbreak: only double \\n (already normal text) -- left alone", r);
}

static void test_unbreak_cyrillic_glues_without_locale(void)
{
    /* Regression: is_word_char() used to rely on iswalnum(), which
     * depends on the process locale. setlocale() is never called by
     * the app, so under the default "C" locale iswalnum() returns
     * FALSE for anything outside ASCII -- meaning line-unbreak would
     * silently do nothing on Russian text. */
    WCHAR *r = ub_detect_unbreak(L"\u0421\u043c\u043e\u0442\u0440\u0438\nvot");
    check_eq("unbreak: Cyrillic text is glued without needing setlocale()",
             r, L"\u0421\u043c\u043e\u0442\u0440\u0438 vot");
    free(r);
}

static void test_unbreak_crlf_single_glues_with_space(void)
{
    /* Regression: real Windows clipboard text is CRLF, not bare \n.
     * The old code only recognized \n -- \r fell through as an
     * ordinary character, breaking the before/after lookup on every
     * single line ending and silently disabling the whole detector
     * on real clipboard input. */
    WCHAR *r = ub_detect_unbreak(L"word\r\nword");
    check_eq("unbreak: CRLF between words -> space", r, L"word word");
    free(r);
}

static void test_unbreak_crlf_hyphen_wordbreak(void)
{
    WCHAR *r = ub_detect_unbreak(L"exam-\r\nple");
    check_eq("unbreak: CRLF hyphenated word wrap gets glued", r, L"example");
    free(r);
}

static void test_unbreak_crlf_double_preserved(void)
{
    WCHAR *r = ub_detect_unbreak(L"word\r\n\r\nword\r\nword");
    check_eq("unbreak: CRLF CRLF (paragraph boundary) kept, single CRLF glued",
             r, L"word\r\n\r\nword word");
    free(r);
}

static void test_unbreak_crlf_cyrillic(void)
{
    WCHAR *r = ub_detect_unbreak(L"\u042d\u0442\u043e \u0431\u044b\u043b \u0438\u043d\u0442\u0435\u0440\u0435\u0441-\r\n\u043d\u044b\u0439 \u044d\u043a\u0441\u043f\u0435\u0440\u0438\u043c\u0435\u043d\u0442.");
    check_eq("unbreak: CRLF hyphen-break on Cyrillic text",
             r, L"\u042d\u0442\u043e \u0431\u044b\u043b \u0438\u043d\u0442\u0435\u0440\u0435\u0441\u043d\u044b\u0439 \u044d\u043a\u0441\u043f\u0435\u0440\u0438\u043c\u0435\u043d\u0442.");
    free(r);
}

/* ---- config ---- */

static void test_config_defaults(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    check_true("config: netcheck enabled by default", cfg.netcheck_enabled);
    check_true("config: default timeout is 3000ms", cfg.netcheck_timeout_ms == 3000);
    check_false("config: autorun disabled by default", cfg.autorun_enabled);
    check_true("config: no extra trackers by default", cfg.extra_tracker_count == 0);
    ub_config_free(&cfg);
}

static void test_config_parses_known_keys(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    ub_config_parse(
        L"# comment\n"
        L"netcheck_enabled=false\n"
        L"netcheck_timeout_ms=5000\n"
        L"autorun_enabled=true\n",
        &cfg);
    check_false("config: netcheck_enabled=false parsed", cfg.netcheck_enabled);
    check_true("config: netcheck_timeout_ms=5000 parsed", cfg.netcheck_timeout_ms == 5000);
    check_true("config: autorun_enabled=true parsed", cfg.autorun_enabled);
    ub_config_free(&cfg);
}

static void test_config_ignores_unknown_keys_and_blank_lines(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    ub_config_parse(
        L"\n"
        L"some_future_key=whatever\n"
        L"   \n"
        L"netcheck_enabled=false\n",
        &cfg);
    check_false("config: known key still applies alongside unknown ones", cfg.netcheck_enabled);
    ub_config_free(&cfg);
}

static void test_config_collects_repeated_extra_trackers(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    ub_config_parse(
        L"extra_tracker_param=ref\n"
        L"extra_tracker_param=my_custom_param\n",
        &cfg);
    check_true("config: two extra_tracker_param lines collected", cfg.extra_tracker_count == 2);
    if (cfg.extra_tracker_count == 2) {
        check_true("config: first extra tracker is 'ref'", wcscmp(cfg.extra_trackers[0], L"ref") == 0);
        check_true("config: second extra tracker is 'my_custom_param'",
                   wcscmp(cfg.extra_trackers[1], L"my_custom_param") == 0);
    }
    ub_config_free(&cfg);
}

static void test_config_bool_accepts_yes_no_1_0(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    ub_config_parse(L"autorun_enabled=yes\n", &cfg);
    check_true("config: 'yes' parses as TRUE", cfg.autorun_enabled);
    ub_config_free(&cfg);

    ub_config_defaults(&cfg);
    ub_config_parse(L"autorun_enabled=0\n", &cfg);
    check_false("config: '0' parses as FALSE", cfg.autorun_enabled);
    ub_config_free(&cfg);
}

static void test_config_garbage_timeout_keeps_default(void)
{
    ub_config_t cfg;
    ub_config_defaults(&cfg);
    ub_config_parse(L"netcheck_timeout_ms=not_a_number\n", &cfg);
    check_true("config: unparseable timeout falls back to default", cfg.netcheck_timeout_ms == 3000);
    ub_config_free(&cfg);
}

static void test_url_extra_tracker_from_config_gets_stripped(void)
{
    static const WCHAR *extra[] = { L"ref", L"my_custom_param" };
    ub_url_set_extra_trackers(extra, 2);

    WCHAR *r = ub_detect_url(L"https://example.com/?id=1&ref=partner&my_custom_param=x");
    check_eq("url: extra tracker from config gets stripped alongside built-ins",
             r, L"https://example.com/?id=1");
    free(r);

    ub_url_set_extra_trackers(NULL, 0); /* reset so later tests see default behavior */
}

/* ---- netcheck decision (pure logic, no real network) ---- */

static void test_netcheck_reverts_on_4xx(void)
{
    check_true("netcheck: reverts on 404", ub_netcheck_should_revert(TRUE, 404));
}

static void test_netcheck_reverts_on_5xx(void)
{
    check_true("netcheck: reverts on 500", ub_netcheck_should_revert(TRUE, 500));
}

static void test_netcheck_keeps_on_2xx(void)
{
    check_false("netcheck: keeps cleaned link on 200", ub_netcheck_should_revert(TRUE, 200));
}

static void test_netcheck_keeps_on_request_failure(void)
{
    /* Timeout/DNS/connection failure is ambiguous -- could be a flaky
     * network, not evidence the stripped params were needed. Revert
     * only on a definitive bad-status response. */
    check_false("netcheck: does not revert on a failed request (timeout/DNS/etc)",
                ub_netcheck_should_revert(FALSE, 0));
}

/* ---- pipeline (integration) ---- */

static void test_pipeline_json_passthrough(void)
{
    WCHAR *r = ub_pipeline_run_sync(L"{\"a\":1}");
    check_eq("pipeline: json gets formatted through the whole pipeline",
             r, L"{\n  \"a\": 1\n}");
    free(r);
}

static void test_pipeline_wrapped_url_with_softwrap(void)
{
    /* Realistic case from the bug report: a tracked link plus text with
     * a soft line break in the same buffer. unbreak should glue the
     * line first, and url should strip the trackers afterwards. */
    WCHAR *r = ub_pipeline_run_sync(
        L"Check this\nhttps://www.youtube.com/watch?v=dQw4w9WgXcQ&utm_source=telegram&si=abc123xyz");
    check_eq("pipeline: line break glued and trackers stripped in one pass",
             r, L"Check this https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    free(r);
}

static void test_pipeline_unchanged_text_is_copy(void)
{
    WCHAR *r = ub_pipeline_run_sync(L"ordinary text, nothing to see here");
    check_eq("pipeline: if no detector applies, result is just a copy of the input",
             r, L"ordinary text, nothing to see here");
    free(r);
}

int main(void)
{
    test_url_bare_strips_trackers();
    test_url_embedded_in_text();
    test_url_markdown_wrapped();
    test_url_multiple_in_text();
    test_url_no_query_untouched();
    test_url_no_url_untouched();
    test_url_keeps_non_tracker_params();

    test_json_pretty_prints();
    test_json_rejects_plain_text();

    test_unbreak_single_newline_glues_with_space();
    test_unbreak_double_newline_preserved();
    test_unbreak_hyphen_wordbreak();
    test_unbreak_list_marker_not_glued();
    test_unbreak_list_marker_not_glued_with_other_change();
    test_unbreak_space_before_newline_regression();
    test_unbreak_multi_spaces_collapse();
    test_unbreak_no_single_newline_untouched();
    test_unbreak_cyrillic_glues_without_locale();
    test_unbreak_crlf_single_glues_with_space();
    test_unbreak_crlf_hyphen_wordbreak();
    test_unbreak_crlf_double_preserved();
    test_unbreak_crlf_cyrillic();

    test_config_defaults();
    test_config_parses_known_keys();
    test_config_ignores_unknown_keys_and_blank_lines();
    test_config_collects_repeated_extra_trackers();
    test_config_bool_accepts_yes_no_1_0();
    test_config_garbage_timeout_keeps_default();
    test_url_extra_tracker_from_config_gets_stripped();

    test_netcheck_reverts_on_4xx();
    test_netcheck_reverts_on_5xx();
    test_netcheck_keeps_on_2xx();
    test_netcheck_keeps_on_request_failure();

    test_pipeline_json_passthrough();
    test_pipeline_wrapped_url_with_softwrap();
    test_pipeline_unchanged_text_is_copy();

    printf("\n%d/%d passed\n", g_total - g_failed, g_total);
    return g_failed == 0 ? 0 : 1;
}
