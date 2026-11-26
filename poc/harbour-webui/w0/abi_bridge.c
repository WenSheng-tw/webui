/*
 * abi_bridge.c  --  bridge side of the PoC-002 W0 ABI test (strict ISO C89).
 *
 * Declares WebUI exactly as the bridge will (bridge_decl.h: ubool results, int bool and
 * enum parameters, size_t ids, mirrored webui_event_t) and drives the real static library
 * with it. Output is TAP. Reference values come from abi_real.c, which sees the real
 * webui.h; 64-bit integers go through abi_i64.c.
 *
 *   abi_test --headless             groups A (runtime part), C, D (no display needed)
 *   abi_test --live --dir <w0 dir>  groups E, D2/D3 live part (needs DISPLAY and a browser)
 *
 * Build: gcc -std=c89 -pedantic-errors -Wall -Wextra -c abi_bridge.c
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bridge_decl.h"
#include "bridge_num.h"

/* Mirrored event layout (the bridge reads these fields directly). */
struct webui_event_t
{
    size_t window;
    size_t event_type;
    char *element;
    size_t event_number;
    size_t bind_id;
    size_t client_id;
    size_t connection_id;
    char *cookies;
};

/* Assertions that need nothing but the compiler. */
#define SA(name, cond) typedef char sa_##name[(cond) ? 1 : -1]
SA(ubool_is_1_byte, sizeof(ubool) == 1);
SA(int_is_4_bytes, sizeof(int) == 4);
SA(size_t_is_pointer, sizeof(size_t) == sizeof(void *));
SA(event_is_8_words, sizeof(struct webui_event_t) == 8 * sizeof(void *));

/* abi_real.c (C99, real webui.h) */
extern void abi_real_sizes(size_t *o);
extern void abi_real_event_layout(size_t *o);
extern int abi_real_enum_count(void);
extern int abi_real_enum_get(int i, const char **name, int *bridge, int *real);
extern int abi_real_set_root_folder(size_t w, const char *p);
extern int abi_real_set_default_root_folder(const char *p);
extern int abi_real_set_port(size_t w, size_t port);
extern int abi_real_is_shown(size_t w);
extern int abi_real_browser_exist(size_t b);
extern int abi_real_is_app_running(void);
extern int abi_real_is_high_contrast(void);
extern void abi_real_set_public(size_t w, int v);
extern void abi_lock(void);
extern void abi_unlock(void);
extern unsigned long abi_thread_id(void);
extern void abi_sleep_ms(unsigned ms);
extern unsigned long abi_now_ms(void);
extern int abi_have_display(void);
extern int abi_listen_tcp(int *fd);
extern void abi_close_fd(int fd);
extern int abi_tcp_listener(unsigned port, char *addr, size_t addrsize);
extern int abi_http_get(unsigned port, const char *path, char *buf, size_t bufsize);
extern unsigned long abi_dirty_call(void (*fn)(void), size_t a, size_t b, size_t c);
extern int abi_dirty_available(void);
extern ubool abi_probe_al_only(void);

/* abi_i64.c (C99) */
extern int bridge_get_int_at(void *e, size_t index, double *num, char *str, size_t strsize);
extern int bridge_get_int(void *e, double *num, char *str, size_t strsize);
extern int bridge_interface_get_int_at(size_t window, size_t event_number, size_t index, double *num, char *str, size_t strsize);
extern int bridge_return_int(void *e, double d);
extern int abi_i64_convert_decimal(const char *dec, double *num, char *str, size_t strsize);
extern int abi_i64_from_double(double d, char *dec, size_t decsize);
extern void abi_i64_return_decimal(void *e, const char *dec);
extern void abi_i64_get_int_at_decimal(void *e, size_t index, char *dec, size_t decsize);
extern void abi_i64_get_int_decimal(void *e, char *dec, size_t decsize);
extern void abi_i64_interface_get_int_at_decimal(size_t window, size_t event_number, size_t index, char *dec, size_t decsize);
extern size_t abi_i64_sizeof_long_long(void);
extern void abi_i64_limits(char *min, char *max, size_t size);

/* TAP */
static int g_n = 0;
static int g_bad = 0;
static char g_msg[768];

static void tap(int cond, const char *desc)
{
    g_n++;
    if (!cond)
    {
        g_bad++;
    }
    printf("%s %d - %s\n", cond ? "ok" : "not ok", g_n, desc);
    fflush(stdout);
}

static void tap_info(const char *desc)
{
    g_n++;
    printf("ok %d - %s # informational\n", g_n, desc);
    fflush(stdout);
}

static void diag(const char *text)
{
    printf("# %s\n", text);
    fflush(stdout);
}

static unsigned long ul(size_t v)
{
    return (unsigned long) v;
}

/* ------------------------------------------------------------------ group A */

static void group_a(void)
{
    size_t sz[6];
    size_t real[9];
    size_t mine[9];
    int i;
    int bad;
    int count;
    const char *name;
    int bv;
    int rv;

    abi_real_sizes(sz);
    sprintf(g_msg, "A1 real header: bool=%lu config=%lu size_t=%lu void*=%lu long long=%lu int=%lu",
            ul(sz[0]), ul(sz[1]), ul(sz[2]), ul(sz[3]), ul(sz[4]), ul(sz[5]));
    diag(g_msg);
    tap(sz[0] == sizeof(ubool), "A1 sizeof(real bool) equals sizeof(ubool)");
    tap(sz[1] == sizeof(int), "A1 sizeof(webui_config) equals sizeof(int)");
    tap(sz[2] == sizeof(size_t) && sz[3] == sizeof(void *), "A1 size_t has pointer width in both units");
    tap(sz[4] == 8 && abi_i64_sizeof_long_long() == 8, "A1 long long is 8 bytes (real header unit and int64 unit)");
    tap(sizeof(int) == sz[5], "A1 int has the same size in both units");

    abi_real_event_layout(real);
    mine[0] = sizeof(struct webui_event_t);
    mine[1] = offsetof(struct webui_event_t, window);
    mine[2] = offsetof(struct webui_event_t, event_type);
    mine[3] = offsetof(struct webui_event_t, element);
    mine[4] = offsetof(struct webui_event_t, event_number);
    mine[5] = offsetof(struct webui_event_t, bind_id);
    mine[6] = offsetof(struct webui_event_t, client_id);
    mine[7] = offsetof(struct webui_event_t, connection_id);
    mine[8] = offsetof(struct webui_event_t, cookies);
    bad = 0;
    for (i = 0; i < 9; i++)
    {
        if (real[i] != mine[i])
        {
            bad++;
            sprintf(g_msg, "layout slot %d: real %lu mirrored %lu", i, ul(real[i]), ul(mine[i]));
            diag(g_msg);
        }
    }
    sprintf(g_msg, "A2 mirrored webui_event_t equals the real one (size %lu, 8 offsets, %d mismatches)", ul(mine[0]), bad);
    tap(bad == 0, g_msg);

    count = abi_real_enum_count();
    bad = 0;
    for (i = 0; i < count; i++)
    {
        if (!abi_real_enum_get(i, &name, &bv, &rv) || bv != rv)
        {
            bad++;
            sprintf(g_msg, "enum %s: bridge %d real %d", name, bv, rv);
            diag(g_msg);
        }
    }
    sprintf(g_msg, "A3 %d enumerators: bridge constants equal the real header values (%d mismatches)", count, bad);
    tap(count == 32 && bad == 0, g_msg);
}

/* ------------------------------------------------------------------ group C */

#define FN(f) ((void (*)(void)) (f))

enum
{
    K_W_STR,
    K_STR,
    K_W_SZ,
    K_W,
    K_SZ,
    K_VOID
};

struct c1_case
{
    const char *name;
    int kind;
    void (*direct)(void);
    void (*real)(void);
    size_t a;
    const char *s;
    size_t b;
    int expect;
    int iterations;
};

static unsigned long call_direct(const struct c1_case *c)
{
    switch (c->kind)
    {
    case K_W_STR:
        return ((ubool(*)(size_t, const char *)) c->direct)(c->a, c->s);
    case K_STR:
        return ((ubool(*)(const char *)) c->direct)(c->s);
    case K_W_SZ:
        return ((ubool(*)(size_t, size_t)) c->direct)(c->a, c->b);
    case K_W:
    case K_SZ:
        return ((ubool(*)(size_t)) c->direct)(c->a);
    default:
        return ((ubool(*)(void)) c->direct)();
    }
}

static unsigned long call_real(const struct c1_case *c)
{
    switch (c->kind)
    {
    case K_W_STR:
        return (unsigned long) ((int (*)(size_t, const char *)) c->real)(c->a, c->s);
    case K_STR:
        return (unsigned long) ((int (*)(const char *)) c->real)(c->s);
    case K_W_SZ:
        return (unsigned long) ((int (*)(size_t, size_t)) c->real)(c->a, c->b);
    case K_W:
    case K_SZ:
        return (unsigned long) ((int (*)(size_t)) c->real)(c->a);
    default:
        return (unsigned long) ((int (*)(void)) c->real)();
    }
}

static unsigned long call_dirty(const struct c1_case *c)
{
    switch (c->kind)
    {
    case K_W_STR:
        return abi_dirty_call(c->direct, c->a, (size_t) c->s, 0);
    case K_STR:
        return abi_dirty_call(c->direct, (size_t) c->s, 0, 0);
    case K_W_SZ:
        return abi_dirty_call(c->direct, c->a, c->b, 0);
    case K_W:
    case K_SZ:
        return abi_dirty_call(c->direct, c->a, 0, 0);
    default:
        return abi_dirty_call(c->direct, 0, 0, 0);
    }
}

static void run_c1(const struct c1_case *c)
{
    int i;
    int mismatch = 0;
    int out_of_range = 0;
    int dirty_upper = 0;
    int int_read_wrong = 0;
    unsigned long d;
    unsigned long r;
    unsigned long x;

    for (i = 0; i < c->iterations; i++)
    {
        d = call_direct(c);
        r = call_real(c);
        if (d > 1UL)
        {
            out_of_range++;
        }
        if (d != (unsigned long) c->expect || r != (unsigned long) c->expect || d != r)
        {
            mismatch++;
        }
        if (abi_dirty_available())
        {
            x = call_dirty(c);
            if ((x & 0xFFUL) > 1UL || (x & 0xFFUL) != (unsigned long) c->expect)
            {
                mismatch++;
            }
            if ((x >> 8) != 0UL)
            {
                dirty_upper++;
            }
            if ((unsigned long) (unsigned int) x != (unsigned long) c->expect)
            {
                int_read_wrong++;
            }
        }
    }
    sprintf(g_msg, "C1 %s -> %d: %d iterations, %d mismatches, %d outside {0,1}", c->name, c->expect, c->iterations, mismatch, out_of_range);
    tap(mismatch == 0 && out_of_range == 0, g_msg);
    sprintf(g_msg, "C1 %s negative control: upper bytes of rax left dirty %d times, reading the result as int would be wrong %d times",
            c->name, dirty_upper, int_read_wrong);
    tap_info(g_msg);
}

static void c1_set(struct c1_case *c, const char *name, int kind, void (*direct)(void), void (*real)(void),
                   size_t a, const char *s, size_t b, int expect, int iterations)
{
    c->name = name;
    c->kind = kind;
    c->direct = direct;
    c->real = real;
    c->a = a;
    c->s = s;
    c->b = b;
    c->expect = expect;
    c->iterations = iterations;
}

static int wait_listener(size_t w, char *addr, size_t addrsize)
{
    unsigned long t0 = abi_now_ms();
    size_t port;

    while (abi_now_ms() - t0 < 10000UL)
    {
        port = webui_get_port(w);
        if (port != 0 && abi_tcp_listener((unsigned) port, addr, addrsize))
        {
            return (int) port;
        }
        abi_sleep_ms(50);
    }
    return 0;
}

static void group_c(void)
{
    struct c1_case c;
    size_t w;
    size_t free_port;
    int fd = -1;
    int used_port;
    char buf[4096];
    int i;
    int cfg;

    w = webui_new_window();
    sprintf(g_msg, "C0 webui_new_window returned %lu", ul(w));
    tap(w != 0, g_msg);
    sprintf(g_msg, "C0 dirty-call trampoline available: %d (x86-64 Linux only)", abi_dirty_available());
    tap_info(g_msg);

    if (abi_dirty_available())
    {
        unsigned long x = abi_dirty_call(FN(abi_probe_al_only), 0, 0, 0);
        ubool direct = abi_probe_al_only();

        sprintf(g_msg, "C1 harness control: a function that sets only %%al returned rax=0x%lx; low byte %lu, upper bits dirty: %d, int read equals 1: %d, ubool read equals 1: %d",
                x, x & 0xFFUL, (x >> 8) != 0UL, (unsigned int) x == 1U, (int) direct == 1);
        tap((x & 0xFFUL) == 1UL && (x >> 8) != 0UL && (unsigned int) x != 1U && direct == 1, g_msg);
    }
    c1_set(&c, "set_root_folder(\"/tmp\")", K_W_STR, FN(webui_set_root_folder), FN(abi_real_set_root_folder), w, "/tmp", 0, 1, 1000);
    run_c1(&c);
    c1_set(&c, "set_root_folder(\"/nonexistent_abi\")", K_W_STR, FN(webui_set_root_folder), FN(abi_real_set_root_folder), w, "/nonexistent_abi", 0, 0, 1000);
    run_c1(&c);
    c1_set(&c, "set_default_root_folder(\"/tmp\")", K_STR, FN(webui_set_default_root_folder), FN(abi_real_set_default_root_folder), 0, "/tmp", 0, 1, 1000);
    run_c1(&c);
    c1_set(&c, "set_default_root_folder(\"/nonexistent_abi\")", K_STR, FN(webui_set_default_root_folder), FN(abi_real_set_default_root_folder), 0, "/nonexistent_abi", 0, 0, 1000);
    run_c1(&c);

    used_port = abi_listen_tcp(&fd);
    sprintf(g_msg, "C1 helper listener holds port %d", used_port);
    tap(used_port > 0, g_msg);
    if (used_port > 0)
    {
        c1_set(&c, "set_port(port in use)", K_W_SZ, FN(webui_set_port), FN(abi_real_set_port), w, NULL, (size_t) used_port, 0, 1000);
        run_c1(&c);
        abi_close_fd(fd);
    }
    free_port = webui_get_free_port();
    sprintf(g_msg, "C1 webui_get_free_port returned %lu", ul(free_port));
    tap(free_port > 0 && free_port <= 65535, g_msg);
    c1_set(&c, "set_port(free port)", K_W_SZ, FN(webui_set_port), FN(abi_real_set_port), w, NULL, free_port, 1, 1000);
    run_c1(&c);

    c1_set(&c, "is_shown(never shown window)", K_W, FN(webui_is_shown), FN(abi_real_is_shown), w, NULL, 0, 0, 1000);
    run_c1(&c);
    c1_set(&c, "browser_exist(Safari)", K_SZ, FN(webui_browser_exist), FN(abi_real_browser_exist), BRIDGE_BROWSER_SAFARI, NULL, 0, 0, 1000);
    run_c1(&c);
    {
        int have_chrome = abi_real_browser_exist(BRIDGE_BROWSER_CHROME);

        if (have_chrome)
        {
            c1_set(&c, "browser_exist(Chrome)", K_SZ, FN(webui_browser_exist), FN(abi_real_browser_exist), BRIDGE_BROWSER_CHROME, NULL, 0, 1, 1000);
            run_c1(&c);
        }
        else
        {
            tap_info("C1 browser_exist(Chrome) has no true case: no Chrome found by WebUI on this machine");
        }
    }
    c1_set(&c, "interface_is_app_running (before first show)", K_VOID, FN(webui_interface_is_app_running), FN(abi_real_is_app_running), 0, NULL, 0, 0, 1000);
    run_c1(&c);
    c1_set(&c, "is_high_contrast", K_VOID, FN(webui_is_high_contrast), FN(abi_real_is_high_contrast), 0, NULL, 0, 0, 20);
    run_c1(&c);

    /* C2: bool parameter passed in an int slot, observed through the listener address. */
    {
        size_t wa = webui_new_window();
        size_t wb = webui_new_window();
        size_t wc = webui_new_window();
        size_t wd = webui_new_window();
        size_t we = webui_new_window();
        char aa[80], ab[80], ac[80], ad[80], ae[80];
        int pa, pb, pc, pd, pe;

        webui_set_public(wa, 1);
        webui_set_public(wb, 0);
        abi_real_set_public(wc, 1);
        abi_real_set_public(wd, 0);
        webui_set_public(we, 256);
        webui_start_server(wa, "<html><body>a</body></html>");
        webui_start_server(wb, "<html><body>b</body></html>");
        webui_start_server(wc, "<html><body>c</body></html>");
        webui_start_server(wd, "<html><body>d</body></html>");
        webui_start_server(we, "<html><body>e</body></html>");
        pa = wait_listener(wa, aa, sizeof(aa));
        pb = wait_listener(wb, ab, sizeof(ab));
        pc = wait_listener(wc, ac, sizeof(ac));
        pd = wait_listener(wd, ad, sizeof(ad));
        pe = wait_listener(we, ae, sizeof(ae));
        sprintf(g_msg, "C2 set_public(int 1) listens on %s (port %d), expected 00000000 (0.0.0.0)", aa, pa);
        tap(pa > 0 && strcmp(aa, "00000000") == 0, g_msg);
        sprintf(g_msg, "C2 set_public(int 0) listens on %s (port %d), expected 0100007F (127.0.0.1)", ab, pb);
        tap(pb > 0 && strcmp(ab, "0100007F") == 0, g_msg);
        sprintf(g_msg, "C2 set_public(real bool true) listens on %s, identical to the int call (%s)", ac, aa);
        tap(pc > 0 && strcmp(ac, aa) == 0, g_msg);
        sprintf(g_msg, "C2 set_public(real bool false) listens on %s, identical to the int call (%s)", ad, ab);
        tap(pd > 0 && strcmp(ad, ab) == 0, g_msg);
        sprintf(g_msg, "C2 set_public(int 256) listens on %s (port %d): documented unsupported, callee reads the low byte only", ae, pe);
        tap_info(g_msg);

        /* C3 (headless part): use_cookies is observable on a served page. */
        webui_set_config(BRIDGE_CFG_USE_COOKIES, 1);
        buf[0] = '\0';
        i = abi_http_get((unsigned) pb, "/", buf, sizeof(buf));
        sprintf(g_msg, "C3 use_cookies=1: GET / returned %d bytes, Set-Cookie webui_auth present: %d", i, strstr(buf, "webui_auth=") != NULL);
        tap(i > 0 && strstr(buf, "webui_auth=") != NULL, g_msg);
        webui_set_config(BRIDGE_CFG_USE_COOKIES, 0);
        buf[0] = '\0';
        i = abi_http_get((unsigned) pd, "/", buf, sizeof(buf));
        sprintf(g_msg, "C3 use_cookies=0: GET / returned %d bytes, Set-Cookie webui_auth present: %d", i, strstr(buf, "webui_auth=") != NULL);
        tap(i > 0 && strstr(buf, "webui_auth=") == NULL, g_msg);
        webui_set_config(BRIDGE_CFG_USE_COOKIES, 1);
    }

    /* C3: every webui_config value is accepted through the int declaration. */
    for (cfg = BRIDGE_CFG_SHOW_WAIT_CONNECTION; cfg <= BRIDGE_CFG_ASYNCHRONOUS_RESPONSE; cfg++)
    {
        webui_set_config(cfg, 1);
        webui_set_config(cfg, 0);
    }
    webui_set_config(BRIDGE_CFG_SHOW_WAIT_CONNECTION, 1);
    webui_set_config(BRIDGE_CFG_USE_COOKIES, 1);
    tap(1, "C3 webui_set_config(int, int) called with all 6 options set and cleared, no crash, defaults restored");
    diag("C3 folder_monitor, multi_client, ui_event_blocking, asynchronous_response and show_wait_connection effects are observed in group E");

    /* C4: pointer results through the ABI (informational). */
    {
        char *enc = webui_encode("hello");
        char *dec;

        tap(enc != NULL && strcmp(enc, "aGVsbG8=") == 0, "C4 webui_encode(\"hello\") == \"aGVsbG8=\"");
        dec = webui_decode(enc != NULL ? enc : "");
        tap(dec != NULL && strcmp(dec, "hello") == 0, "C4 webui_decode round trip == \"hello\"");
        webui_free(enc);
        webui_free(dec);
        tap(strcmp(webui_get_mime_type("index.html"), "text/html") == 0, "C4 webui_get_mime_type(\"index.html\") == \"text/html\"");
        tap(webui_get_last_error_number() == 0 && strcmp(webui_get_last_error_message(), "") == 0, "C4 last error stub is 0 and empty string");
    }
}

/* ------------------------------------------------------------------ group D */

static void group_d_headless(void)
{
    static const double ok_values[] = {0.0, 1.0, 65535.0, 2147483647.0, 4294967295.0, 9007199254740992.0};
    static const char *ok_names[] = {"0", "1", "65535", "2^31-1", "2^32-1", "2^53"};
    volatile double zero = 0.0;
    double nan_v = zero / zero;
    double inf_v = 1.0 / zero;
    double bad_values[8];
    const char *bad_names[8];
    size_t out;
    double back;
    int i;
    int bad;
    size_t ids[200];
    int dup;
    int j;
    size_t wid;
    size_t newid;
    char dec[48];
    char str[48];
    double num;
    char min[32];
    char max[32];

    bad = 0;
    for (i = 0; i < 6; i++)
    {
        out = 12345;
        back = -1.0;
        if (!bridge_n2size(ok_values[i], &out) || (double) out != ok_values[i] || !bridge_size2n(out, &back) || back != ok_values[i])
        {
            bad++;
            sprintf(g_msg, "round trip failed for %s", ok_names[i]);
            diag(g_msg);
        }
    }
    sprintf(g_msg, "D1 size_t/double round trip exact for 0, 1, 65535, 2^31-1, 2^32-1, 2^53 (%d failures)", bad);
    tap(bad == 0, g_msg);

    bad_values[0] = 9007199254740994.0;
    bad_names[0] = "2^53+2";
    bad_values[1] = 18446744073709551616.0;
    bad_names[1] = "2^64";
    bad_values[2] = -1.0;
    bad_names[2] = "-1";
    bad_values[3] = nan_v;
    bad_names[3] = "NaN";
    bad_values[4] = 1.5;
    bad_names[4] = "1.5";
    bad_values[5] = inf_v;
    bad_names[5] = "+Inf";
    bad_values[6] = -inf_v;
    bad_names[6] = "-Inf";
    bad_values[7] = -0.5;
    bad_names[7] = "-0.5";
    bad = 0;
    for (i = 0; i < 8; i++)
    {
        out = 777;
        if (bridge_n2size(bad_values[i], &out) || out != 777)
        {
            bad++;
            sprintf(g_msg, "%s was accepted", bad_names[i]);
            diag(g_msg);
        }
    }
    sprintf(g_msg, "D1 rejected without conversion: 2^53+2, 2^64, -1, NaN, 1.5, +-Inf, -0.5 (%d accepted)", bad);
    tap(bad == 0, g_msg);
    tap(!bridge_size2n((size_t) -1, &back), "D1 size2n refuses SIZE_MAX (above 2^53) instead of rounding");
    tap(bridge_n2size(-0.0, &out) && out == 0, "D1 negative zero is accepted as 0");

    /* D2 (headless part): window ids */
    bad = 0;
    dup = 0;
    for (i = 0; i < 200; i++)
    {
        ids[i] = webui_new_window();
        if (ids[i] == 0 || ids[i] > 65535 || !bridge_size2n(ids[i], &back) || !bridge_n2size(back, &out) || out != ids[i])
        {
            bad++;
        }
        for (j = 0; j < i; j++)
        {
            if (ids[j] == ids[i])
            {
                dup++;
            }
        }
    }
    sprintf(g_msg, "D2 200 windows: ids in 1..65535, exact through double, %d out of range, %d duplicates (first %lu last %lu)",
            bad, dup, ul(ids[0]), ul(ids[199]));
    tap(bad == 0 && dup == 0, g_msg);
    wid = webui_new_window_id(60000);
    newid = webui_get_new_window_id();
    sprintf(g_msg, "D2 new_window_id(60000) = %lu, get_new_window_id() = %lu", ul(wid), ul(newid));
    tap(wid == 60000 && newid > 0 && newid <= 65535, g_msg);
    sprintf(g_msg, "D2 process ids of a window without a browser: parent %lu child %lu, free port %lu",
            ul(webui_get_parent_process_id(ids[0])), ul(webui_get_child_process_id(ids[0])), ul(webui_get_free_port()));
    tap(webui_get_free_port() <= 65535, g_msg);

    /* D3 (headless part): 64-bit helpers */
    abi_i64_limits(min, max, sizeof(min));
    bad = 0;
    {
        static const char *exact[] = {"0", "-1", "1", "9007199254740992", "-9007199254740992", "123456789012"};
        for (i = 0; i < 6; i++)
        {
            num = -1.0;
            if (abi_i64_convert_decimal(exact[i], &num, str, sizeof(str)) != 0 || num != atof(exact[i]))
            {
                bad++;
            }
            if (!abi_i64_from_double(num, dec, sizeof(dec)) || strcmp(dec, exact[i]) != 0)
            {
                bad++;
            }
        }
    }
    sprintf(g_msg, "D3 int64 values within 2^53 are exact numbers and round trip to the same decimal text (%d failures)", bad);
    tap(bad == 0, g_msg);
    bad = 0;
    {
        static const char *over[] = {"9007199254740993", "-9007199254740993", "9223372036854775807", "-9223372036854775808"};
        for (i = 0; i < 4; i++)
        {
            if (abi_i64_convert_decimal(over[i], &num, str, sizeof(str)) != 1 || strcmp(str, over[i]) != 0)
            {
                bad++;
            }
        }
    }
    sprintf(g_msg, "D3 int64 beyond 2^53 is signalled as overflow and returned as exact decimal text (%d failures)", bad);
    tap(bad == 0, g_msg);
    tap(strcmp(max, "9223372036854775807") == 0 && strcmp(min, "-9223372036854775808") == 0, "D3 LLONG_MAX and LLONG_MIN print as expected");
    bad = 0;
    if (abi_i64_from_double(9007199254740994.0, dec, sizeof(dec)) || abi_i64_from_double(1.5, dec, sizeof(dec)) ||
        abi_i64_from_double(nan_v, dec, sizeof(dec)) || abi_i64_from_double(-9007199254740994.0, dec, sizeof(dec)))
    {
        bad++;
    }
    sprintf(g_msg, "D3 double to int64 refuses values above 2^53, fractions and NaN (%d accepted)", bad);
    tap(bad == 0, g_msg);
}

/* ------------------------------------------------------------------ group E */

#define QUEUE_MAX 256

struct async_item
{
    void *e;
    size_t window;
    size_t event_number;
    char element[32];
    char arg[64];
};

static struct
{
    unsigned long main_tid;
    size_t window;

    int f_seen;
    size_t f_count;
    int f_i0_status;
    double f_i0;
    char f_s1[64];
    int f_b2;
    double f_f3;
    char f_elem[32];
    size_t f_window;
    size_t f_type;
    size_t f_evnum;
    size_t f_bind;
    size_t f_client;
    size_t f_conn;
    unsigned long f_tid;

    int i_seen;
    int i_status[4];
    char i_at[4][32];
    char i_iface[4][32];
    char i_str[4][32];
    char i_first[32];

    int n_count;
    size_t n_max;
    size_t n_min;
    int n_bad;
    size_t n_max_bind;
    size_t n_max_client;

    int slow_inflight;
    int slow_max;

    struct async_item queue[QUEUE_MAX];
    int q_head;
    int q_tail;
    int async_overflow;
} G;

static void copy_str(char *dst, size_t size, const char *src)
{
    if (src == NULL)
    {
        src = "";
    }
    strncpy(dst, src, size - 1);
    dst[size - 1] = '\0';
}

static void on_f(webui_event_t *e)
{
    double d;
    char tmp[48];
    int st;

    abi_lock();
    G.f_count = webui_get_count(e);
    st = bridge_get_int_at(e, 0, &d, tmp, sizeof(tmp));
    G.f_i0_status = st;
    G.f_i0 = d;
    copy_str(G.f_s1, sizeof(G.f_s1), webui_get_string_at(e, 1));
    G.f_b2 = webui_get_bool_at(e, 2);
    G.f_f3 = webui_get_float_at(e, 3);
    copy_str(G.f_elem, sizeof(G.f_elem), e->element);
    G.f_window = e->window;
    G.f_type = e->event_type;
    G.f_evnum = e->event_number;
    G.f_bind = e->bind_id;
    G.f_client = e->client_id;
    G.f_conn = e->connection_id;
    G.f_tid = abi_thread_id();
    G.f_seen = 1;
    abi_unlock();
    webui_return_string(e, "f-ok");
}

static void on_i(webui_event_t *e)
{
    double d;
    char tmp[48];
    size_t k;

    abi_lock();
    for (k = 0; k < 4; k++)
    {
        G.i_status[k] = bridge_get_int_at(e, k, &d, G.i_str[k], sizeof(G.i_str[k]));
        abi_i64_get_int_at_decimal(e, k, G.i_at[k], sizeof(G.i_at[k]));
        abi_i64_interface_get_int_at_decimal(e->window, e->event_number, k, G.i_iface[k], sizeof(G.i_iface[k]));
    }
    abi_i64_get_int_decimal(e, G.i_first, sizeof(G.i_first));
    (void) bridge_get_int(e, &d, tmp, sizeof(tmp));
    G.i_seen = 1;
    abi_unlock();
    webui_return_string(e, "i-ok");
}

static void on_n(webui_event_t *e)
{
    double d;

    abi_lock();
    G.n_count++;
    if (!bridge_size2n(e->event_number, &d) || d > 2147483647.0)
    {
        G.n_bad++;
    }
    if (e->event_number > G.n_max)
    {
        G.n_max = e->event_number;
    }
    if (G.n_count == 1 || e->event_number < G.n_min)
    {
        G.n_min = e->event_number;
    }
    if (e->bind_id > G.n_max_bind)
    {
        G.n_max_bind = e->bind_id;
    }
    if (e->client_id > G.n_max_client)
    {
        G.n_max_client = e->client_id;
    }
    abi_unlock();
    webui_return_string(e, "");
}

static void on_slow(webui_event_t *e)
{
    abi_lock();
    G.slow_inflight++;
    if (G.slow_inflight > G.slow_max)
    {
        G.slow_max = G.slow_inflight;
    }
    abi_unlock();
    abi_sleep_ms(400);
    abi_lock();
    G.slow_inflight--;
    abi_unlock();
    webui_return_string(e, "s-ok");
}

/* Asynchronous trampoline: queue (event, window, number, element, first argument) and return. */
static void on_async(webui_event_t *e)
{
    struct async_item *it;
    int next;

    abi_lock();
    next = (G.q_tail + 1) % QUEUE_MAX;
    if (next == G.q_head)
    {
        G.async_overflow++;
        abi_unlock();
        webui_return_string(e, "overflow");
        return;
    }
    it = &G.queue[G.q_tail];
    it->e = e;
    it->window = e->window;
    it->event_number = e->event_number;
    copy_str(it->element, sizeof(it->element), e->element);
    copy_str(it->arg, sizeof(it->arg), webui_get_string_at(e, 0));
    G.q_tail = next;
    abi_unlock();
}

static struct
{
    int g_ok;
    int g_data_valid;
    int h_calls;
    int j_calls;
} A;

static int pump_async(void)
{
    struct async_item it;
    int have;
    int handled = 0;
    const char *data;
    static const char *dec[4] = {"9223372036854775807", "-9223372036854775808", "0", "-1"};
    int k;

    for (;;)
    {
        abi_lock();
        have = G.q_head != G.q_tail;
        if (have)
        {
            it = G.queue[G.q_head];
            G.q_head = (G.q_head + 1) % QUEUE_MAX;
        }
        abi_unlock();
        if (!have)
        {
            return handled;
        }
        handled++;
        if (strcmp(it.element, "abi_g") == 0)
        {
            data = webui_interface_get_string_at(it.window, it.event_number, 0);
            A.g_data_valid = data != NULL && strcmp(data, "argG") == 0;
            abi_sleep_ms(300);
            webui_interface_set_response(it.window, it.event_number, "late");
            A.g_ok = 1;
        }
        else if (strcmp(it.element, "abi_h") == 0)
        {
            k = atoi(it.arg);
            A.h_calls++;
            if (k >= 0 && k < 4)
            {
                abi_i64_return_decimal(it.e, dec[k]);
            }
            else if (k == 4)
            {
                if (!bridge_return_int(it.e, 42.0))
                {
                    webui_return_string(it.e, "rejected-42");
                }
            }
            else
            {
                if (!bridge_return_int(it.e, 9007199254740994.0))
                {
                    webui_return_string(it.e, "rejected");
                }
            }
        }
        else if (strcmp(it.element, "abi_j") == 0)
        {
            k = atoi(it.arg);
            A.j_calls++;
            if (k == 0)
            {
                webui_return_bool(it.e, 1);
            }
            else if (k == 1)
            {
                webui_return_bool(it.e, 0);
            }
            else
            {
                webui_return_float(it.e, 2.5);
            }
        }
        else
        {
            webui_return_string(it.e, "unknown");
        }
    }
}

static int js_flag(size_t w, const char *name)
{
    char script[96];
    char buf[16];

    sprintf(script, "return (window.%s===1)?'1':'0';", name);
    if (!webui_script(w, script, 5, buf, sizeof(buf)))
    {
        return 0;
    }
    return buf[0] == '1';
}

static int wait_flag(size_t w, const char *name, unsigned timeout_ms)
{
    unsigned long t0 = abi_now_ms();

    while (abi_now_ms() - t0 < timeout_ms)
    {
        pump_async();
        if (js_flag(w, name))
        {
            return 1;
        }
        abi_sleep_ms(40);
    }
    return 0;
}

static void js_start(size_t w, const char *fn)
{
    char script[96];

    sprintf(script, "setTimeout(function(){%s();},0);", fn);
    webui_run(w, script);
}

static void js_result(size_t w, const char *var, char *buf, size_t size)
{
    char script[96];

    sprintf(script, "return String(window.%s);", var);
    buf[0] = '\0';
    if (!webui_script(w, script, 5, buf, size))
    {
        buf[0] = '\0';
    }
}

static void group_e(const char *dir)
{
    size_t w;
    char buf[1024];
      unsigned long t0;
    unsigned long t1;
    size_t bind_f;
    int shown_at_return;
    int shown_later;
    int b0;
    int ok;
    int i;
    ubool r;

    memset(&G, 0, sizeof(G));
    memset(&A, 0, sizeof(A));
    G.main_tid = abi_thread_id();

    w = webui_new_window();
    G.window = w;
    sprintf(g_msg, "E0 DISPLAY set, window %lu created, root folder %s", ul(w), dir);
    tap(abi_have_display() && w != 0 && webui_set_root_folder(w, dir), g_msg);

    bind_f = webui_bind(w, "abi_f", on_f);
    ok = bind_f != 0 && webui_bind(w, "abi_i", on_i) != 0 && webui_bind(w, "abi_n", on_n) != 0 && webui_bind(w, "abi_slow", on_slow) != 0;
    sprintf(g_msg, "E0 four binds registered through webui_bind with a static trampoline (first bind id %lu)", ul(bind_f));
    tap(ok, g_msg);

    webui_set_config(BRIDGE_CFG_SHOW_WAIT_CONNECTION, 0);
    t0 = abi_now_ms();
    r = webui_show(w, "abi_live.html");
    t1 = abi_now_ms();
    shown_at_return = webui_is_shown(w);
    sprintf(g_msg, "E0 show with show_wait_connection=0 returned %d after %lu ms, is_shown right after: %d", (int) r, t1 - t0, shown_at_return);
    tap(r == 1, g_msg);
    sprintf(g_msg, "C3 show_wait_connection=0: show returned before the browser connected (is_shown=%d at return)", shown_at_return);
    tap(shown_at_return == 0, g_msg);
    shown_later = 0;
    t0 = abi_now_ms();
    while (abi_now_ms() - t0 < 30000UL)
    {
        if (webui_is_shown(w))
        {
            shown_later = 1;
            break;
        }
        abi_sleep_ms(50);
    }
    sprintf(g_msg, "C3 show_wait_connection=0: window connected later (is_shown=%d after %lu ms)", shown_later, abi_now_ms() - t0);
    tap(shown_later, g_msg);
    webui_set_config(BRIDGE_CFG_SHOW_WAIT_CONNECTION, 1);
    tap(webui_interface_is_app_running() == 1, "C1 interface_is_app_running after the first show is 1 (real bool read through ubool)");

    ok = wait_flag(w, "__ready", 30000);
    tap(ok, "E0 page loaded webui.js and signalled readiness");
    if (!ok)
    {
        diag("E aborted: the browser page never became ready");
        return;
    }

    /* E2 */
    memset(buf, 'X', sizeof(buf));
    ok = webui_script(w, "return 40+2;", 5, buf, 64);
    sprintf(g_msg, "E2 webui_script(\"return 40+2;\") returned %d, buffer \"%.200s\"", ok, buf);
    tap(ok == 1 && strcmp(buf, "42") == 0, g_msg);
    memset(buf, 'X', 64);
    buf[63] = '\0';
    ok = webui_script(w, "throw new Error('abi');", 5, buf, 64);
    sprintf(g_msg, "E2 throwing script returned %d, buffer was zeroed first and now holds the error text \"%.200s\"", ok, buf);
    tap(ok == 0 && strstr(buf, "abi") != NULL && strchr(buf, 'X') == NULL, g_msg);
    memset(buf, 'X', 64);
    buf[63] = '\0';
    t0 = abi_now_ms();
    ok = webui_script(w, "var t = Date.now(); while (Date.now() - t < 1600) {} return 'late';", 1, buf, 64);
    t1 = abi_now_ms();
    b0 = (int) buf[0];
    abi_sleep_ms(1500);
    sprintf(g_msg, "E2 timeout 1 s: returned %d after %lu ms, buffer zeroed (first byte %d) and still empty 1.5 s later (byte %d)",
            ok, t1 - t0, b0, (int) buf[0]);
    tap(ok == 0 && b0 == 0 && buf[0] == '\0' && t1 - t0 < 1500UL, g_msg);
    ok = webui_script(w, "return 'abcdefghij';", 5, buf, 5);
    sprintf(g_msg, "E2 result longer than the buffer is truncated to size-1: \"%.200s\"", buf);
    tap(ok == 1 && strcmp(buf, "abcd") == 0, g_msg);

    /* E1 + D3 live */
    js_start(w, "p1");
    ok = wait_flag(w, "__done1", 20000);
    tap(ok, "E1 page finished the synchronous calls f and i");
    js_result(w, "__r1", buf, sizeof(buf));
    sprintf(g_msg, "E1 JS received \"%.200s\"", buf);
    tap(strcmp(buf, "f-ok|i-ok") == 0, g_msg);
    abi_lock();
    sprintf(g_msg, "E1 callback saw %lu arguments (JS passed 4 after the name: 123, 'x', true, 1.5)", ul(G.f_count));
    tap(G.f_seen && G.f_count == 4, g_msg);
    sprintf(g_msg, "E1 get_int_at(0) = %.0f (status %d), get_string_at(1) = \"%s\", get_bool_at(2) = %d, get_float_at(3) = %g",
            G.f_i0, G.f_i0_status, G.f_s1, G.f_b2, G.f_f3);
    tap(G.f_i0 == 123.0 && G.f_i0_status == 0 && strcmp(G.f_s1, "x") == 0 && G.f_b2 == 1 && G.f_f3 == 1.5, g_msg);
    sprintf(g_msg, "E1 mirrored struct: element \"%s\", window %lu (expected %lu), event_type %lu (expected 4), bind %lu (expected %lu)",
            G.f_elem, ul(G.f_window), ul(w), ul(G.f_type), ul(G.f_bind), ul(bind_f));
    tap(strcmp(G.f_elem, "abi_f") == 0 && G.f_window == w && G.f_type == 4 && G.f_bind == bind_f, g_msg);
    sprintf(g_msg, "E1 callback ran on thread %lu, main thread is %lu", G.f_tid, G.main_tid);
    tap(G.f_tid != G.main_tid, g_msg);
    sprintf(g_msg, "D2 live ids: event_number %lu, client_id %lu, connection_id %lu, bind_id %lu all <= 65535 or < 2^31",
            ul(G.f_evnum), ul(G.f_client), ul(G.f_conn), ul(G.f_bind));
    tap(G.f_evnum < 2147483648UL && G.f_client <= 65535 && G.f_conn <= 65535 && G.f_bind <= 65535, g_msg);
    {
        static const char *want[4] = {"9223372036854775807", "-9223372036854775808", "0", "-1"};
        static const int want_status[4] = {1, 1, 0, 0};
        int bad_at = 0;
        int bad_if = 0;
        int bad_st = 0;

        for (i = 0; i < 4; i++)
        {
            if (strcmp(G.i_at[i], want[i]) != 0)
            {
                bad_at++;
            }
            if (strcmp(G.i_iface[i], want[i]) != 0)
            {
                bad_if++;
            }
            if (G.i_status[i] != want_status[i] || (want_status[i] == 1 && strcmp(G.i_str[i], want[i]) != 0))
            {
                bad_st++;
            }
        }
        sprintf(g_msg, "D3 get_int_at returned LLONG_MAX, LLONG_MIN, 0, -1 exactly (%d wrong); interface_get_int_at identical (%d wrong)", bad_at, bad_if);
        tap(G.i_seen && bad_at == 0 && bad_if == 0, g_msg);
        sprintf(g_msg, "D3 bridge helper: overflow flag and decimal text correct for all 4 values (%d wrong), get_int = %s", bad_st, G.i_first);
        tap(bad_st == 0 && strcmp(G.i_first, want[0]) == 0, g_msg);
    }
    abi_unlock();

    /* C3 ui_event_blocking and multi_client */
    js_start(w, "pslow");
    ok = wait_flag(w, "__done_slow", 20000);
    abi_lock();
    sprintf(g_msg, "C3 ui_event_blocking=0: two simultaneous events ran concurrently (max concurrency %d, expected 2)", G.slow_max);
    tap(ok && G.slow_max == 2, g_msg);
    G.slow_max = 0;
    abi_unlock();
    webui_set_config(BRIDGE_CFG_UI_EVENT_BLOCKING, 1);
    js_start(w, "pslow2");
    ok = wait_flag(w, "__done_slow2", 20000);
    abi_lock();
    sprintf(g_msg, "C3 ui_event_blocking=1: events were serialized (max concurrency %d, expected 1)", G.slow_max);
    tap(ok && G.slow_max == 1, g_msg);
    abi_unlock();
    webui_set_config(BRIDGE_CFG_UI_EVENT_BLOCKING, 0);

    webui_set_config(BRIDGE_CFG_MULTI_CLIENT, 1);
    ok = webui_script(w, "return 1;", 5, buf, 16);
    sprintf(g_msg, "C3 multi_client=1: webui_script returns %d (documented: false), buffer \"%.200s\"", ok, buf);
    tap(ok == 0, g_msg);
    webui_set_config(BRIDGE_CFG_MULTI_CLIENT, 0);
    ok = webui_script(w, "return 1;", 5, buf, 16);
    tap(ok == 1, "C3 multi_client=0 again: webui_script returns 1");

    /* D2 live: event numbers */
    t0 = abi_now_ms();
    js_start(w, "pflood");
    ok = wait_flag(w, "__done_flood", 120000);
    t1 = abi_now_ms();
    abi_lock();
    sprintf(g_msg, "D2 %d live events in %lu ms: event_number min %lu max %lu, %d not exact below 2^31, max bind_id %lu, max client_id %lu",
            G.n_count, t1 - t0, ul(G.n_min), ul(G.n_max), G.n_bad, ul(G.n_max_bind), ul(G.n_max_client));
    tap(ok && G.n_count == 10000 && G.n_bad == 0 && G.n_max_bind <= 65535 && G.n_max_client <= 65535, g_msg);
    abi_unlock();

    /* E3: asynchronous response from the main thread */
    ok = webui_bind(w, "abi_g", on_async) != 0 && webui_bind(w, "abi_h", on_async) != 0 && webui_bind(w, "abi_j", on_async) != 0;
    tap(ok, "E3 three asynchronous binds registered");
    webui_set_config(BRIDGE_CFG_ASYNCHRONOUS_RESPONSE, 1);
    js_start(w, "p3");
    ok = wait_flag(w, "__done3", 30000);
    tap(ok, "E3 page finished the asynchronous calls");
    js_result(w, "__r3", buf, sizeof(buf));
    sprintf(g_msg, "E3 JS received %.200s", buf);
    diag(g_msg);
    sprintf(g_msg, "E3 late response delivered, event data valid until the response (get_string_at on the main thread matched: %d)", A.g_data_valid);
    tap(A.g_ok && A.g_data_valid && strstr(buf, "\"late\"") != NULL, g_msg);
    {
        double dt = 0.0;
        const char *p = strstr(buf, "\"dt\":");

        if (p != NULL)
        {
            dt = atof(p + 5);
        }
        sprintf(g_msg, "E3 the response arrived after the main thread's 300 ms delay (JS measured %.0f ms)", dt);
        tap(dt >= 250.0, g_msg);
    }
    tap(strstr(buf, "\"h\":[\"9223372036854775807\",\"-9223372036854775808\",\"0\",\"-1\",\"42\",\"rejected\"]") != NULL,
        "D3 return_int delivered LLONG_MAX, LLONG_MIN, 0, -1 as exact decimal text; double 42 as \"42\"; 2^53+2 rejected before sending");
    tap(strstr(buf, "\"j\":[\"1\",\"0\",\"2.500000\"]") != NULL,
        "E3 return_bool(int 1), return_bool(int 0) and return_float(2.5) reached JS as \"1\", \"0\", \"2.500000\"");
    webui_set_config(BRIDGE_CFG_ASYNCHRONOUS_RESPONSE, 0);
    sprintf(g_msg, "E3 async queue overflow count %d, h calls %d, j calls %d", G.async_overflow, A.h_calls, A.j_calls);
    tap(G.async_overflow == 0, g_msg);

    webui_close(w);
}

/* ------------------------------------------------------------------ main */

int main(int argc, char **argv)
{
    int live = 0;
    const char *dir = ".";
    int i;

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--live") == 0)
        {
            live = 1;
        }
        else if (strcmp(argv[i], "--headless") == 0)
        {
            live = 0;
        }
        else if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc)
        {
            dir = argv[++i];
        }
        else
        {
            fprintf(stderr, "usage: %s [--headless | --live --dir <w0 dir>]\n", argv[0]);
            return 2;
        }
    }

    printf("TAP version 13\n");
    printf("# mode: %s\n", live ? "live (groups E, C3 effects, D2, D3)" : "headless (groups A, C, D)");
    if (!live)
    {
        group_a();
        group_c();
        group_d_headless();
    }
    else
    {
        if (!abi_have_display())
        {
            printf("1..0 # SKIP no DISPLAY, run under xvfb-run\n");
            return 0;
        }
        group_e(dir);
    }
    webui_exit();
    webui_clean();
    printf("1..%d\n", g_n);
    if (g_bad)
    {
        printf("# FAILED %d of %d\n", g_bad, g_n);
        return 1;
    }
    printf("# all %d passed\n", g_n);
    return 0;
}
