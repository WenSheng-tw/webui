/*
 * abi_real.c  --  reference side of the W0 ABI test (C99, real include/webui.h).
 *
 * Everything the C89 unit cannot or must not do itself lives here: the real prototypes
 * (real bool, real enums, real struct), compile-time size assertions, POSIX helpers
 * (threads, sockets, /proc) and the register-dirtying call trampoline of group C.
 * Build: gcc -std=c99 -Iinclude -I<w0> -c abi_real.c
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include "webui.h"
#include "bridge_consts.h"

/* A1: compile-time facts about the real header */
#define ABI_SA(name, cond) typedef char abi_sa_##name[(cond) ? 1 : -1]
ABI_SA(bool_is_1_byte, sizeof(bool) == 1);
ABI_SA(config_is_int, sizeof(webui_config) == sizeof(int));
ABI_SA(size_t_is_pointer, sizeof(size_t) == sizeof(void *));
ABI_SA(long_long_is_8, sizeof(long long) == 8);
ABI_SA(event_has_8_words, sizeof(webui_event_t) == 8 * sizeof(void *));

void abi_real_sizes(size_t *o)
{
    o[0] = sizeof(bool);
    o[1] = sizeof(webui_config);
    o[2] = sizeof(size_t);
    o[3] = sizeof(void *);
    o[4] = sizeof(long long);
    o[5] = sizeof(int);
}

void abi_real_event_layout(size_t *o)
{
    o[0] = sizeof(webui_event_t);
    o[1] = offsetof(webui_event_t, window);
    o[2] = offsetof(webui_event_t, event_type);
    o[3] = offsetof(webui_event_t, element);
    o[4] = offsetof(webui_event_t, event_number);
    o[5] = offsetof(webui_event_t, bind_id);
    o[6] = offsetof(webui_event_t, client_id);
    o[7] = offsetof(webui_event_t, connection_id);
    o[8] = offsetof(webui_event_t, cookies);
}

/* A3: bridge constants against the real enumerators */
struct abi_enum_row
{
    const char *name;
    int bridge;
    int real;
};

static const struct abi_enum_row s_enum_rows[] = {
#define ABI_ENUM(b, r) {#b, b, (int) (r)},
#include "abi_enums_gen.h"
#undef ABI_ENUM
};

int abi_real_enum_count(void)
{
    return (int) (sizeof(s_enum_rows) / sizeof(s_enum_rows[0]));
}

int abi_real_enum_get(int i, const char **name, int *bridge, int *real)
{
    if (i < 0 || i >= abi_real_enum_count())
    {
        return 0;
    }
    *name = s_enum_rows[i].name;
    *bridge = s_enum_rows[i].bridge;
    *real = s_enum_rows[i].real;
    return 1;
}

/* Reference calls through the real prototypes: bool results and parameters are real bool. */
int abi_real_set_root_folder(size_t w, const char *p) { return webui_set_root_folder(w, p); }
int abi_real_set_default_root_folder(const char *p) { return webui_set_default_root_folder(p); }
int abi_real_set_port(size_t w, size_t port) { return webui_set_port(w, port); }
int abi_real_is_shown(size_t w) { return webui_is_shown(w); }
int abi_real_browser_exist(size_t b) { return webui_browser_exist(b); }
int abi_real_is_app_running(void) { return webui_interface_is_app_running(); }
int abi_real_is_high_contrast(void) { return webui_is_high_contrast(); }
void abi_real_set_public(size_t w, int v) { webui_set_public(w, v != 0); }

/* Threads and time */
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

void abi_lock(void) { pthread_mutex_lock(&s_mutex); }
void abi_unlock(void) { pthread_mutex_unlock(&s_mutex); }
unsigned long abi_thread_id(void) { return (unsigned long) pthread_self(); }

void abi_sleep_ms(unsigned ms)
{
    struct timespec ts;

    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long) (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
    {
    }
}

unsigned long abi_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long) ts.tv_sec * 1000UL + (unsigned long) ts.tv_nsec / 1000000UL;
}

int abi_have_display(void)
{
    const char *d = getenv("DISPLAY");

    return d != NULL && d[0] != '\0';
}

/* Sockets and /proc */
int abi_listen_tcp(int *fd)
{
    struct sockaddr_in a;
    socklen_t len = sizeof(a);
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
    {
        return 0;
    }
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    if (bind(s, (struct sockaddr *) &a, sizeof(a)) != 0 || listen(s, 4) != 0 ||
        getsockname(s, (struct sockaddr *) &a, &len) != 0)
    {
        close(s);
        return 0;
    }
    *fd = s;
    return (int) ntohs(a.sin_port);
}

void abi_close_fd(int fd) { close(fd); }

/* 1 if a TCP listener on port exists; addr receives the local address in /proc hex form. */
int abi_tcp_listener(unsigned port, char *addr, size_t addrsize)
{
    static const char *files[] = {"/proc/net/tcp", "/proc/net/tcp6"};
    char line[512];
    char local[64];
    unsigned p;
    unsigned st;
    size_t i;

    for (i = 0; i < 2; i++)
    {
        FILE *f = fopen(files[i], "r");

        if (f == NULL)
        {
            continue;
        }
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (sscanf(line, " %*d: %63[0-9A-Fa-f]:%x %*[0-9A-Fa-f:] %x", local, &p, &st) == 3 && p == port && st == 0x0A)
            {
                snprintf(addr, addrsize, "%s%s", local, i == 1 ? "(v6)" : "");
                fclose(f);
                return 1;
            }
        }
        fclose(f);
    }
    return 0;
}

int abi_http_get(unsigned port, const char *path, char *buf, size_t bufsize)
{
    struct sockaddr_in a;
    struct timeval tv;
    char req[256];
    size_t used = 0;
    ssize_t n;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
    {
        return -1;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((unsigned short) port);
    if (connect(s, (struct sockaddr *) &a, sizeof(a)) != 0)
    {
        close(s);
        return -1;
    }
    snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: localhost:%u\r\nConnection: close\r\n\r\n", path, port);
    if (write(s, req, strlen(req)) < 0)
    {
        close(s);
        return -1;
    }
    while (used + 1 < bufsize && (n = read(s, buf + used, bufsize - 1 - used)) > 0)
    {
        used += (size_t) n;
    }
    buf[used] = '\0';
    close(s);
    return (int) used;
}

/*
 * Calls fn with up to three integer arguments after loading a poison value into the whole
 * of rax, and returns rax untouched. A callee that sets only %al (setcc, mov $1,%al) leaves
 * the upper bytes poisoned, which is exactly what a bool result may legally do.
 */
#if defined(__x86_64__) && defined(__linux__)
__asm__(
    ".text\n"
    ".globl abi_dirty_call\n"
    ".type abi_dirty_call,@function\n"
    "abi_dirty_call:\n"
    "    pushq %rbx\n"
    "    movq %rdi, %r11\n"
    "    movq %rsi, %rdi\n"
    "    movq %rdx, %rsi\n"
    "    movq %rcx, %rdx\n"
    "    movabsq $0xDEADBEEFCAFEBABE, %rax\n"
    "    callq *%r11\n"
    "    popq %rbx\n"
    "    ret\n"
    ".size abi_dirty_call, .-abi_dirty_call\n");
int abi_dirty_available(void) { return 1; }

/* Harness control: a function that, like a bool result, defines only %al. */
__asm__(
    ".text\n"
    ".globl abi_probe_al_only\n"
    ".type abi_probe_al_only,@function\n"
    "abi_probe_al_only:\n"
    "    movb $1, %al\n"
    "    ret\n"
    ".size abi_probe_al_only, .-abi_probe_al_only\n");
#else
unsigned long abi_dirty_call(void (*fn)(void), size_t a, size_t b, size_t c)
{
    return ((size_t (*)(size_t, size_t, size_t)) fn)(a, b, c);
}
int abi_dirty_available(void) { return 0; }
unsigned char abi_probe_al_only(void) { return 1; }
#endif
