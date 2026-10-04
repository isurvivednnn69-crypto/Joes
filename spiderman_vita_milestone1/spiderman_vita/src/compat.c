/* bionic -> newlib shims for libSpiderMan.so. Milestone 1: enough to link and boot-log.
 * Anything not listed here is routed to unresolved_stub(), which logs the caller address. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <time.h>
#include <setjmp.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <malloc.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include "so_util.h"
#include "compat.h"

/* ---------- path remapping ---------- */
#define PKG "com.gameloft.android.ANMP.GloftAMHM"
static int starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }
static const char *remap(const char *p, char *out, size_t n) {
  if (!p) return p;
  if (starts(p, "/sdcard/Android/data/" PKG "/files")) { snprintf(out, n, DATA_PATH "%s", p + strlen("/sdcard/Android/data/" PKG "/files")); return out; }
  if (starts(p, "/sdcard/Android/obb/" PKG))            { snprintf(out, n, DATA_PATH "%s", p + strlen("/sdcard/Android/obb/" PKG)); return out; }
  if (starts(p, "/data/data/" PKG))                     { snprintf(out, n, DATA_PATH "/internal%s", p + strlen("/data/data/" PKG)); return out; }
  if (starts(p, "/sdcard"))                             { snprintf(out, n, DATA_PATH "/sdcard%s", p + 7); return out; }
  return p;
}
#define R(p) char _rb[512]; p = remap(p, _rb, sizeof(_rb))

/* ---------- bionic data objects ---------- */
static char fake_sF[3 * 88];
static char bionic_ctype[257];
static short bionic_tolower[257];
static uintptr_t fake_stack_guard = 0x42424242;
static char fake_dso_handle;

static void init_ctype(void) {
  bionic_ctype[0] = 0; bionic_tolower[0] = -1;
  for (int c = 0; c < 256; c++) {
    int f = 0;
    if (c >= 'A' && c <= 'Z') f |= 0x01;
    if (c >= 'a' && c <= 'z') f |= 0x02;
    if (c >= '0' && c <= '9') f |= 0x04;
    if (c == ' ' || (c >= 9 && c <= 13)) f |= 0x08;
    if (c < 128 && ispunct(c)) f |= 0x10;
    if (c < 32 || c == 127) f |= 0x20;
    if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) f |= 0x40;
    if (c == ' ') f |= 0x80;
    bionic_ctype[c + 1] = (char)f;
    bionic_tolower[c + 1] = (c >= 'A' && c <= 'Z') ? c + 32 : c;
  }
}

static FILE *mapf(void *f) {
  uintptr_t a = (uintptr_t)f, b = (uintptr_t)fake_sF;
  if (a >= b && a < b + sizeof(fake_sF)) { uintptr_t i = (a - b) / 84; return i == 0 ? stdin : (i == 1 ? stdout : stderr); }
  return (FILE *)f;
}

/* ---------- misc / logging ---------- */
static int android_log_vprint(int prio, const char *tag, const char *fmt, va_list ap) {
  char b[1024]; vsnprintf(b, sizeof(b), fmt, ap); logf_("[A/%d %s] %s\n", prio, tag ? tag : "", b); return 0;
}
static void assert2(const char *file, int line, const char *func, const char *expr) {
  logf_("ASSERT %s:%d %s: %s\n", file, line, func, expr); abort();
}
static int cxa_atexit(void *fn, void *arg, void *dso) { return 0; }
static int *errno_fn(void) { return &errno; }
static int ret0(void) { return 0; }
static int retm1(void) { return -1; }
static void *retnull(void) { return NULL; }

static void unresolved_stub(void) {
  logf_("CALL TO UNRESOLVED IMPORT from %p\n", __builtin_return_address(0));
  sceKernelExitProcess(0);
}
uintptr_t unresolved_stub_addr(void) { return (uintptr_t)&unresolved_stub; }

static uintptr_t stack_chk_fail(void) { logf_("stack smashing detected\n"); sceKernelExitProcess(0); return 0; }

/* ---------- exception unwinding support ---------- */
static so_module *g_mod;
void compat_set_module(so_module *m) { g_mod = m; init_ctype(); }
static uintptr_t gnu_unwind_find_exidx(uintptr_t pc, int *pcount) {
  if (g_mod && g_mod->exidx && so_addr_in_text(g_mod, pc)) { *pcount = g_mod->exidx_size / 8; return g_mod->exidx; }
  *pcount = 0; return 0;
}

/* ---------- time (bionic time_t / timeval / timespec are 32-bit) ---------- */
typedef struct { int32_t tv_sec, tv_nsec; } b_timespec;
typedef struct { int32_t tv_sec, tv_usec; } b_timeval;
typedef struct { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; long tm_gmtoff; const char *tm_zone; } b_tm;

static int32_t time_w(int32_t *t) { int32_t v = (int32_t)time(NULL); if (t) *t = v; return v; }
static int gettimeofday_w(b_timeval *tv, void *tz) { struct timeval h; gettimeofday(&h, NULL); if (tv) { tv->tv_sec = (int32_t)h.tv_sec; tv->tv_usec = (int32_t)h.tv_usec; } return 0; }
static int clock_gettime_w(int id, b_timespec *ts) {
  uint64_t us = sceKernelGetProcessTimeWide();
  if (id == 0) { struct timeval h; gettimeofday(&h, NULL); ts->tv_sec = (int32_t)h.tv_sec; ts->tv_nsec = h.tv_usec * 1000; return 0; }
  ts->tv_sec = (int32_t)(us / 1000000); ts->tv_nsec = (int32_t)((us % 1000000) * 1000); return 0;
}
static int nanosleep_w(const b_timespec *r, b_timespec *rem) { sceKernelDelayThread((uint64_t)r->tv_sec * 1000000 + r->tv_nsec / 1000); return 0; }
static int usleep_w(unsigned us) { sceKernelDelayThread(us); return 0; }
static int sched_yield_w(void) { sceKernelDelayThread(0); return 0; }
static void tm_to_b(const struct tm *h, b_tm *b) { memset(b, 0, sizeof(*b)); memcpy(b, h, 9 * sizeof(int)); }
static void tm_from_b(const b_tm *b, struct tm *h) { memset(h, 0, sizeof(*h)); memcpy(h, b, 9 * sizeof(int)); }
static b_tm *localtime_w(const int32_t *t) { static b_tm out; time_t v = *t; struct tm *h = localtime(&v); tm_to_b(h, &out); return &out; }
static b_tm *gmtime_w(const int32_t *t) { static b_tm out; time_t v = *t; struct tm *h = gmtime(&v); tm_to_b(h, &out); return &out; }
static int32_t mktime_w(b_tm *b) { struct tm h; tm_from_b(b, &h); return (int32_t)mktime(&h); }
static size_t strftime_w(char *s, size_t n, const char *fmt, const b_tm *b) { struct tm h; tm_from_b(b, &h); return strftime(s, n, fmt, &h); }
static int32_t clock_w(void) { return (int32_t)clock(); }

/* ---------- stdio ---------- */
static int fprintf_w(void *f, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vfprintf(mapf(f), fmt, ap); va_end(ap); return r; }
static int vfprintf_w(void *f, const char *fmt, va_list ap) { return vfprintf(mapf(f), fmt, ap); }
static int fputs_w(const char *s, void *f) { return fputs(s, mapf(f)); }
static int fputc_w(int c, void *f) { return fputc(c, mapf(f)); }
static char *fgets_w(char *s, int n, void *f) { return fgets(s, n, mapf(f)); }
static int getc_w(void *f) { return fgetc(mapf(f)); }
static int ungetc_w(int c, void *f) { return ungetc(c, mapf(f)); }
static int fflush_w(void *f) { return fflush(f ? mapf(f) : NULL); }
static int fclose_w(void *f) { return fclose(mapf(f)); }
static size_t fread_w(void *p, size_t s, size_t n, void *f) { return fread(p, s, n, mapf(f)); }
static size_t fwrite_w(const void *p, size_t s, size_t n, void *f) { return fwrite(p, s, n, mapf(f)); }
static int fseek_w(void *f, long o, int w) { return fseek(mapf(f), o, w); }
static long ftell_w(void *f) { return ftell(mapf(f)); }
static void rewind_w(void *f) { rewind(mapf(f)); }
static int setvbuf_w(void *f, char *b, int m, size_t s) { return 0; }
static FILE *fopen_w(const char *p, const char *m) { R(p); FILE *f = fopen(p, m); if (!f) logf_("fopen miss: %s\n", p); return f; }

/* bionic struct stat (ARM, 104 bytes) */
static void stat_to_b(const struct stat *h, uint8_t *b) {
  memset(b, 0, 104);
  uint32_t mode = h->st_mode, nlink = 1; int64_t size = h->st_size; int32_t blksz = 4096; int64_t blocks = (size + 511) / 512;
  uint32_t mt = (uint32_t)h->st_mtime;
  memcpy(b + 16, &mode, 4); memcpy(b + 20, &nlink, 4);
  memcpy(b + 48, &size, 8); memcpy(b + 56, &blksz, 4); memcpy(b + 64, &blocks, 8);
  memcpy(b + 72, &mt, 4); memcpy(b + 80, &mt, 4); memcpy(b + 88, &mt, 4);
}
static int stat_w(const char *p, uint8_t *b) { R(p); struct stat h; int r = stat(p, &h); if (r == 0) stat_to_b(&h, b); return r; }
static int fstat_w(int fd, uint8_t *b) { struct stat h; int r = fstat(fd, &h); if (r == 0) stat_to_b(&h, b); return r; }

static int open_w(const char *p, int fl, int mode) {
  R(p); int h = fl & 3;
  if (fl & 0x40) h |= O_CREAT; if (fl & 0x80) h |= O_EXCL; if (fl & 0x200) h |= O_TRUNC; if (fl & 0x400) h |= O_APPEND;
  int fd = open(p, h, mode);
  if (fd < 0) logf_("open miss: %s\n", p);
  return fd;
}
static pthread_mutex_t pread_mtx = PTHREAD_MUTEX_INITIALIZER;
static int pread_w(int fd, void *buf, size_t n, int32_t off) {
  pthread_mutex_lock(&pread_mtx);
  off_t cur = lseek(fd, 0, SEEK_CUR); lseek(fd, off, SEEK_SET);
  int r = read(fd, buf, n); lseek(fd, cur, SEEK_SET);
  pthread_mutex_unlock(&pread_mtx); return r;
}
static int mkdir_w(const char *p, int m) { R(p); return mkdir(p, 0777); }
static int unlink_w(const char *p) { R(p); return unlink(p); }
static int remove_w(const char *p) { R(p); return remove(p); }
static int rename_w(const char *a, const char *b) { char x[512], y[512]; a = remap(a, x, 512); b = remap(b, y, 512); return rename(a, b); }
static int chdir_w(const char *p) { R(p); return chdir(p); }

typedef struct { uint64_t d_ino; int64_t d_off; uint16_t d_reclen; uint8_t d_type; char d_name[256]; } b_dirent;
static void *opendir_w(const char *p) { R(p); return opendir(p); }
static b_dirent *readdir_w(void *d) {
  static b_dirent out; struct dirent *h = readdir(d); if (!h) return NULL;
  memset(&out, 0, sizeof(out)); strncpy(out.d_name, h->d_name, 255); out.d_reclen = sizeof(out); return &out;
}
static int closedir_w(void *d) { return closedir(d); }

/* mmap: anonymous only for now (file-backed returns MAP_FAILED and is logged) */
static void *mmap_w(void *addr, size_t len, int prot, int flags, int fd, int32_t off) {
  if (flags & 0x20) { void *p = memalign(4096, len); if (p) memset(p, 0, len); return p ? p : (void *)-1; }
  logf_("mmap(file fd=%d len=%u off=%d) -> MAP_FAILED (not supported yet)\n", fd, (unsigned)len, off);
  return (void *)-1;
}
static int munmap_w(void *addr, size_t len) { free(addr); return 0; }

/* ---------- pthread (bionic types are 4 bytes; map to heap objects) ---------- */
static pthread_mutex_t g_init_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t *mtx_get(uint32_t *slot) {
  pthread_mutex_lock(&g_init_mtx);
  uint32_t v = *slot;
  if (v == 0 || v == 0x4000 || v == 0x8000) {
    pthread_mutex_t *m = calloc(1, sizeof(*m)); pthread_mutexattr_t a; pthread_mutexattr_init(&a);
    if (v == 0x4000) pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &a); *slot = (uint32_t)(uintptr_t)m; v = *slot;
  }
  pthread_mutex_unlock(&g_init_mtx);
  return (pthread_mutex_t *)(uintptr_t)v;
}
static int mutex_init_w(uint32_t *slot, const int *attr) {
  pthread_mutex_t *m = calloc(1, sizeof(*m)); pthread_mutexattr_t a; pthread_mutexattr_init(&a);
  if (attr && *attr == 1) pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  int r = pthread_mutex_init(m, &a); *slot = (uint32_t)(uintptr_t)m; return r;
}
static int mutex_destroy_w(uint32_t *slot) { if (*slot > 0x8000) { pthread_mutex_destroy((pthread_mutex_t *)(uintptr_t)*slot); free((void *)(uintptr_t)*slot); *slot = 0; } return 0; }
static int mutex_lock_w(uint32_t *s) { return pthread_mutex_lock(mtx_get(s)); }
static int mutex_trylock_w(uint32_t *s) { return pthread_mutex_trylock(mtx_get(s)); }
static int mutex_unlock_w(uint32_t *s) { return pthread_mutex_unlock(mtx_get(s)); }
static int mutexattr_init_w(int *a) { *a = 0; return 0; }
static int mutexattr_settype_w(int *a, int t) { *a = t; return 0; }
static int mutexattr_destroy_w(int *a) { return 0; }

static pthread_cond_t *cond_get(uint32_t *slot) {
  pthread_mutex_lock(&g_init_mtx);
  if (*slot == 0) { pthread_cond_t *c = calloc(1, sizeof(*c)); pthread_cond_init(c, NULL); *slot = (uint32_t)(uintptr_t)c; }
  pthread_mutex_unlock(&g_init_mtx);
  return (pthread_cond_t *)(uintptr_t)*slot;
}
static int cond_init_w(uint32_t *slot, const void *a) { *slot = 0; cond_get(slot); return 0; }
static int cond_destroy_w(uint32_t *slot) { if (*slot) { pthread_cond_destroy((pthread_cond_t *)(uintptr_t)*slot); free((void *)(uintptr_t)*slot); *slot = 0; } return 0; }
static int cond_signal_w(uint32_t *s) { return pthread_cond_signal(cond_get(s)); }
static int cond_broadcast_w(uint32_t *s) { return pthread_cond_broadcast(cond_get(s)); }
static int cond_wait_w(uint32_t *c, uint32_t *m) { return pthread_cond_wait(cond_get(c), mtx_get(m)); }
static int cond_timedwait_w(uint32_t *c, uint32_t *m, const b_timespec *t) {
  struct timespec h; h.tv_sec = t->tv_sec; h.tv_nsec = t->tv_nsec; return pthread_cond_timedwait(cond_get(c), mtx_get(m), &h);
}

/* thread handles: bionic pthread_t is 4 bytes, host type may be larger -> table of ids */
#define MAX_THR 256
static pthread_t thr_tab[MAX_THR]; static uint8_t thr_used[MAX_THR];
static uint32_t thr_register(pthread_t t) {
  pthread_mutex_lock(&g_init_mtx);
  for (int i = 1; i < MAX_THR; i++) if (!thr_used[i]) { thr_used[i] = 1; thr_tab[i] = t; pthread_mutex_unlock(&g_init_mtx); return i; }
  pthread_mutex_unlock(&g_init_mtx); return 0;
}
typedef struct { void *(*fn)(void *); void *arg; } thr_start;
static void *thr_tramp(void *p) { thr_start s = *(thr_start *)p; free(p); return s.fn(s.arg); }
static int pthread_create_w(uint32_t *out, const uint32_t *attr, void *(*fn)(void *), void *arg) {
  pthread_attr_t a; pthread_attr_init(&a);
  if (attr) { if (attr[2]) pthread_attr_setstacksize(&a, attr[2]); if (attr[0] & 1) pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED); }
  thr_start *s = malloc(sizeof(*s)); s->fn = fn; s->arg = arg;
  pthread_t t; int r = pthread_create(&t, &a, thr_tramp, s);
  if (r == 0) *out = thr_register(t);
  return r;
}
static int pthread_join_w(uint32_t id, void **ret) { if (id == 0 || id >= MAX_THR) return -1; int r = pthread_join(thr_tab[id], ret); thr_used[id] = 0; return r; }
static uint32_t pthread_self_w(void) {
  pthread_t me = pthread_self();
  for (int i = 1; i < MAX_THR; i++) if (thr_used[i] && pthread_equal(thr_tab[i], me)) return i;
  return thr_register(me);
}
static int pthread_equal_w(uint32_t a, uint32_t b) { return a == b; }
static int attr_init_w(uint32_t *a) { memset(a, 0, 36); return 0; }
static int attr_destroy_w(uint32_t *a) { return 0; }
static int attr_setdetach_w(uint32_t *a, int s) { if (s) a[0] |= 1; else a[0] &= ~1u; return 0; }
static int attr_setstack_w(uint32_t *a, size_t s) { a[2] = s; return 0; }
static int once_w(int *o, void (*fn)(void)) {
  pthread_mutex_lock(&g_init_mtx); int run = (*o == 0); if (run) *o = 1; pthread_mutex_unlock(&g_init_mtx);
  if (run) fn(); return 0;
}

/* ---------- misc string helpers missing/unsafe in newlib ---------- */
static size_t strlcat_w(char *d, const char *s, size_t n) {
  size_t dl = strnlen(d, n), sl = strlen(s); if (dl == n) return n + sl;
  size_t c = sl < n - dl - 1 ? sl : n - dl - 1; memcpy(d + dl, s, c); d[dl + c] = 0; return dl + sl;
}
static void *memmem_w(const void *h, size_t hl, const void *n, size_t nl) {
  if (!nl) return (void *)h; if (hl < nl) return NULL;
  for (size_t i = 0; i <= hl - nl; i++) if (!memcmp((const char *)h + i, n, nl)) return (void *)((const char *)h + i);
  return NULL;
}
static int strerror_r_w(int e, char *b, size_t n) { snprintf(b, n, "%s", strerror(e)); return 0; }

/* ---------- import table ---------- */
#define D(n) { #n, (uintptr_t)&n }
#define W(n, f) { n, (uintptr_t)&f }

const so_import compat_imports[] = {
  /* data objects */
  W("__sF", fake_sF), W("_ctype_", bionic_ctype), W("_tolower_tab_", bionic_tolower),
  W("__stack_chk_guard", fake_stack_guard), W("__dso_handle", fake_dso_handle),
  /* android / runtime */
  W("__android_log_vprint", android_log_vprint), W("__assert2", assert2), W("__cxa_atexit", cxa_atexit),
  W("__errno", errno_fn), W("__gnu_Unwind_Find_exidx", gnu_unwind_find_exidx), W("__stack_chk_fail", stack_chk_fail),
  /* math */
  D(acos), D(asin), D(atan), D(atan2), D(ceil), D(cos), D(cosh), D(exp), D(floor), D(fmod), D(frexp), D(ldexp),
  D(log), D(log10), D(modf), D(pow), D(sin), D(sinh), D(sqrt), D(tan), D(tanh),
  /* memory / string / convert */
  D(malloc), D(calloc), D(realloc), D(free), D(abort), D(exit), D(qsort),
  D(memchr), D(memcmp), D(memcpy), D(memmove), D(memset), W("memmem", memmem_w), W("strlcat", strlcat_w),
  D(strcat), D(strchr), D(strcmp), D(strcoll), D(strcpy), D(strdup), D(strlen), D(strncmp), D(strncpy), D(strpbrk),
  D(strrchr), D(strstr), D(strtod), D(strtok), D(strtok_r), D(strtol), D(strtoll), D(strtoul), D(strxfrm),
  D(strcasecmp), D(strncasecmp), D(strerror), W("strerror_r", strerror_r_w), D(atoi), D(atol),
  D(isalnum), D(isalpha), D(isspace), D(isupper), D(isxdigit), D(toupper), D(lrand48), D(srand48),
  D(setlocale), D(getenv),
  /* wide chars */
  D(btowc), D(mbrtowc), D(wcrtomb), D(wcscmp), D(wcscoll), D(wcscpy), D(wcslen), D(wcsxfrm), D(wctob),
  D(wmemchr), D(wmemcmp), D(wmemcpy), D(wmemmove), D(wmemset), D(towlower), D(towupper),
  /* stdio */
  D(printf), D(sprintf), D(snprintf), D(vsnprintf), D(vsprintf), D(sscanf), D(puts), D(perror),
  W("fprintf", fprintf_w), W("vfprintf", vfprintf_w), W("fputs", fputs_w), W("fputc", fputc_w), W("putc", fputc_w),
  W("fgets", fgets_w), W("getc", getc_w), W("ungetc", ungetc_w), W("fflush", fflush_w), W("fclose", fclose_w),
  W("fread", fread_w), W("fwrite", fwrite_w), W("fseek", fseek_w), W("ftell", ftell_w), W("rewind", rewind_w),
  W("setvbuf", setvbuf_w), W("fopen", fopen_w),
  /* files */
  W("open", open_w), W("pread", pread_w), W("stat", stat_w), W("fstat", fstat_w), W("mkdir", mkdir_w),
  W("unlink", unlink_w), W("remove", remove_w), W("rename", rename_w), W("chdir", chdir_w),
  W("opendir", opendir_w), W("readdir", readdir_w), W("closedir", closedir_w),
  D(close), D(read), D(write), D(lseek), D(getcwd),
  W("mmap", mmap_w), W("munmap", munmap_w),
  /* time */
  W("time", time_w), W("gettimeofday", gettimeofday_w), W("clock_gettime", clock_gettime_w), W("nanosleep", nanosleep_w),
  W("usleep", usleep_w), W("sched_yield", sched_yield_w), W("localtime", localtime_w), W("gmtime", gmtime_w),
  W("mktime", mktime_w), W("strftime", strftime_w), W("clock", clock_w),
  /* setjmp */
  D(setjmp), D(longjmp),
  /* pthread */
  W("pthread_mutex_init", mutex_init_w), W("pthread_mutex_destroy", mutex_destroy_w), W("pthread_mutex_lock", mutex_lock_w),
  W("pthread_mutex_trylock", mutex_trylock_w), W("pthread_mutex_unlock", mutex_unlock_w),
  W("pthread_mutexattr_init", mutexattr_init_w), W("pthread_mutexattr_settype", mutexattr_settype_w),
  W("pthread_mutexattr_destroy", mutexattr_destroy_w),
  W("pthread_cond_init", cond_init_w), W("pthread_cond_destroy", cond_destroy_w), W("pthread_cond_signal", cond_signal_w),
  W("pthread_cond_broadcast", cond_broadcast_w), W("pthread_cond_wait", cond_wait_w), W("pthread_cond_timedwait", cond_timedwait_w),
  W("pthread_create", pthread_create_w), W("pthread_join", pthread_join_w), W("pthread_self", pthread_self_w),
  W("pthread_equal", pthread_equal_w), W("pthread_once", once_w),
  W("pthread_attr_init", attr_init_w), W("pthread_attr_destroy", attr_destroy_w),
  W("pthread_attr_setdetachstate", attr_setdetach_w), W("pthread_attr_setstacksize", attr_setstack_w),
  D(pthread_key_create), D(pthread_key_delete), D(pthread_getspecific), D(pthread_setspecific),
  /* harmless stubs: ids, signals, syslog, sockets, ioctl */
  W("getpid", ret0), W("getuid", ret0), W("geteuid", ret0), W("getgid", ret0), W("getegid", ret0), W("getpwuid", retnull),
  W("bsd_signal", ret0), W("sigaction", ret0), W("raise", ret0), W("openlog", ret0), W("closelog", ret0), W("syslog", ret0),
  W("chmod", ret0), W("fcntl", ret0), W("ioctl", retm1), W("poll", retm1), W("select", retm1),
  W("socket", retm1), W("connect", retm1), W("bind", retm1), W("listen", retm1), W("accept", retm1), W("send", retm1),
  W("recv", retm1), W("sendto", retm1), W("recvfrom", retm1), W("shutdown", retm1), W("setsockopt", retm1),
  W("getsockopt", retm1), W("getsockname", retm1), W("getpeername", retm1), W("inet_addr", retm1),
  W("gethostbyname", retnull), W("getservbyname", retnull), W("fdopen", retnull),
};
const size_t compat_imports_count = sizeof(compat_imports) / sizeof(compat_imports[0]);
