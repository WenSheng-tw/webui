/*
 * abi_counters.c  --  allocation counters for the W0 ABI test (C99).
 *
 * Built with -DABI_COUNTERS and linked with -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,
 * --wrap=free, every call made from the test objects and from libwebui-2-static.a is routed
 * through the wrappers below, which keep a table of live blocks. Memory taken inside libc
 * (stdio buffers, dlopen) and by dlopen-ed libraries is not seen. Without the define the
 * file only reports that counting is disabled.
 */

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

int abi_counters_enabled(void);
void abi_counters_get(unsigned long *allocs, unsigned long *frees, unsigned long *live_blocks,
                      unsigned long *live_bytes, unsigned long *foreign_frees, unsigned long *table_full);

#ifndef ABI_COUNTERS

int abi_counters_enabled(void) { return 0; }

void abi_counters_get(unsigned long *allocs, unsigned long *frees, unsigned long *live_blocks,
                      unsigned long *live_bytes, unsigned long *foreign_frees, unsigned long *table_full)
{
    *allocs = *frees = *live_blocks = *live_bytes = *foreign_frees = *table_full = 0;
}

#else

#include <stdlib.h>

extern void *__real_malloc(size_t n);
extern void *__real_calloc(size_t n, size_t m);
extern void *__real_realloc(void *p, size_t n);
extern void __real_free(void *p);

#define SLOTS (1u << 20)
#define TOMB ((void *) 1)

static struct slot
{
    void *p;
    size_t n;
} s_tab[SLOTS];

static pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;
static unsigned long s_allocs, s_frees, s_live, s_bytes, s_foreign, s_full;

static unsigned hash_of(void *p)
{
    return (unsigned) (((uintptr_t) p >> 4) * 2654435761u) & (SLOTS - 1);
}

static void add(void *p, size_t n)
{
    unsigned i = hash_of(p);
    unsigned k;

    for (k = 0; k < SLOTS; k++, i = (i + 1) & (SLOTS - 1))
    {
        if (s_tab[i].p == NULL || s_tab[i].p == TOMB)
        {
            s_tab[i].p = p;
            s_tab[i].n = n;
            s_allocs++;
            s_live++;
            s_bytes += n;
            return;
        }
    }
    s_full++;
}

static void del(void *p)
{
    unsigned i = hash_of(p);
    unsigned k;

    for (k = 0; k < SLOTS && s_tab[i].p != NULL; k++, i = (i + 1) & (SLOTS - 1))
    {
        if (s_tab[i].p == p)
        {
            s_tab[i].p = TOMB;
            s_frees++;
            s_live--;
            s_bytes -= s_tab[i].n;
            return;
        }
    }
    s_foreign++;
}

void *__wrap_malloc(size_t n)
{
    void *p = __real_malloc(n);

    if (p != NULL)
    {
        pthread_mutex_lock(&s_mx);
        add(p, n);
        pthread_mutex_unlock(&s_mx);
    }
    return p;
}

void *__wrap_calloc(size_t n, size_t m)
{
    void *p = __real_calloc(n, m);

    if (p != NULL)
    {
        pthread_mutex_lock(&s_mx);
        add(p, n * m);
        pthread_mutex_unlock(&s_mx);
    }
    return p;
}

void *__wrap_realloc(void *old, size_t n)
{
    void *p = __real_realloc(old, n);

    if (p != NULL || (n == 0 && old != NULL))
    {
        pthread_mutex_lock(&s_mx);
        if (old != NULL)
        {
            del(old);
        }
        if (p != NULL)
        {
            add(p, n);
        }
        pthread_mutex_unlock(&s_mx);
    }
    return p;
}

void __wrap_free(void *p)
{
    if (p != NULL)
    {
        pthread_mutex_lock(&s_mx);
        del(p);
        pthread_mutex_unlock(&s_mx);
    }
    __real_free(p);
}

int abi_counters_enabled(void) { return 1; }

void abi_counters_get(unsigned long *allocs, unsigned long *frees, unsigned long *live_blocks,
                      unsigned long *live_bytes, unsigned long *foreign_frees, unsigned long *table_full)
{
    pthread_mutex_lock(&s_mx);
    *allocs = s_allocs;
    *frees = s_frees;
    *live_blocks = s_live;
    *live_bytes = s_bytes;
    *foreign_frees = s_foreign;
    *table_full = s_full;
    pthread_mutex_unlock(&s_mx);
}

#endif
