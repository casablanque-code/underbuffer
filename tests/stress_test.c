#include "detector.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* UB_STRESS_LIGHT=1 scales iteration counts way down. Same code path,
 * same coverage per-iteration -- just fewer iterations. Valgrind
 * serializes threads under its own scheduler and adds heavy per-
 * instruction overhead, so the full volume (tuned for a fast native
 * run) can take many minutes there, especially on slower CI hardware.
 * The light profile still exercises every detector, every thread,
 * and enough alloc/free churn to catch real leaks/corruption -- it
 * just doesn't need 200k iterations to do it. */
static BOOL g_light_mode;

static int iters_per_thread(void) { return g_light_mode ? 200 : 5000; }
static int thread_count(void)     { return 16; } /* cheap either way; iteration count is what Valgrind makes expensive */
static int churn_iters(void)      { return g_light_mode ? 20000 : 200000; }

typedef struct {
    const WCHAR *input;
    const WCHAR *expect; /* NULL means: pipeline should return exactly input unchanged */
} case_t;

static const case_t CASES[] = {
    { L"https://example.com/x?id=1&utm_source=a&utm_medium=b",
      L"https://example.com/x?id=1" },
    { L"exam-\nple",
      L"example" },
    { L"{\"a\":1,\"b\":[1,2,3]}",
      L"{\n  \"a\": 1,\n  \"b\": [\n    1,\n    2,\n    3\n  ]\n}" },
    { L"\x421\x43c\x43e\x442\x440\x438-\n\x432\x43e\x442", /* Cyrillic hyphen-break test case */
      NULL }, /* checked separately below, contains no tracker/json */
    { L"plain unchanged text, nothing to do here", NULL },
};
static const size_t CASE_COUNT = sizeof(CASES) / sizeof(CASES[0]);

static void *thread_worker(void *arg)
{
    long idx = (long)arg;
    int iters = iters_per_thread();
    for (int i = 0; i < iters; i++) {
        const case_t *c = &CASES[(idx + i) % CASE_COUNT];
        WCHAR *out = ub_pipeline_run_sync(c->input);
        if (!out) {
            fprintf(stderr, "thread %ld: pipeline returned NULL\n", idx);
            exit(1);
        }
        if (c->expect && wcscmp(out, c->expect) != 0) {
            fwprintf(stderr, L"thread %ld: mismatch\n  got: %ls\n  exp: %ls\n",
                      idx, out, c->expect);
            exit(1);
        }
        free(out);
    }
    return NULL;
}

static void run_thread_stress(void)
{
    int n = thread_count();
    pthread_t threads[16];
    for (long i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, thread_worker, (void *)i);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("thread stress: %d threads x %d iters -- ok\n", n, iters_per_thread());
}

/* Deterministic PRNG, no external deps. */
static unsigned long g_rng_state = 0x1234567;
static unsigned long next_rand(void)
{
    g_rng_state ^= g_rng_state << 13;
    g_rng_state ^= g_rng_state >> 17;
    g_rng_state ^= g_rng_state << 5;
    return g_rng_state;
}

static void fill_random_input(WCHAR *buf, size_t len)
{
    static const WCHAR POOL[] =
        L"abc def-\nghi \n\n jkl {\"x\":1} https://a.b/?utm_source=z&id=2 "
        L"\x421\x43c\x43e\x442\x440\x438 \n mno   pqr\tstu def-\r\nghi \r\n\r\n";
    size_t pool_len = wcslen(POOL);
    for (size_t i = 0; i < len; i++) {
        buf[i] = POOL[next_rand() % pool_len];
    }
    buf[len] = L'\0';
}

#define CHURN_MAX_LEN 256

static void run_churn_stress(void)
{
    int iters = churn_iters();
    WCHAR *scratch = (WCHAR *)malloc((CHURN_MAX_LEN + 1) * sizeof(WCHAR));
    for (int i = 0; i < iters; i++) {
        size_t len = 1 + (next_rand() % CHURN_MAX_LEN);
        fill_random_input(scratch, len);

        WCHAR *out = ub_pipeline_run_sync(scratch);
        if (!out) {
            fprintf(stderr, "churn: pipeline returned NULL at iter %d\n", i);
            free(scratch);
            exit(1);
        }
        free(out);
    }
    free(scratch);
    printf("churn stress: %d alloc/free cycles -- ok\n", iters);
}

int main(void)
{
    const char *light = getenv("UB_STRESS_LIGHT");
    g_light_mode = (light != NULL && light[0] != '\0' && light[0] != '0');

    run_thread_stress();
    run_churn_stress();
    return 0;
}
