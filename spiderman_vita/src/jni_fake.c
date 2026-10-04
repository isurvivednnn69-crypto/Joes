#include <stdio.h>
/* Fake JavaVM / JNIEnv. Every function slot logs its index; a few are implemented.
 * The log tells us which Java methods the native code wants, which drives the next milestone. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "so_util.h"
#include "jni_fake.h"

#define NSLOTS 240

/* ids handed out for classes / methods / fields, so call stubs can print their names */
#define MAX_IDS 4096
static char *id_names[MAX_IDS]; static int id_count;
static void *new_id(const char *kind, const char *a, const char *b) {
  char buf[512]; snprintf(buf, sizeof(buf), "%s %s%s%s", kind, a ? a : "", b ? " " : "", b ? b : "");
  if (id_count >= MAX_IDS) return strdup(buf);
  return id_names[id_count++] = strdup(buf);
}
static const char *id_name(void *p) { for (int i = 0; i < id_count; i++) if (id_names[i] == p) return id_names[i]; return NULL; }

static void unimpl(int n, uintptr_t a1, uintptr_t a2) {
  const char *nm = id_name((void *)a2);
  logf_("JNI slot %d called%s%s\n", n, nm ? " on " : "", nm ? nm : "");
}

#define JNI_STUB(n) static uintptr_t jni_s##n(uintptr_t env, uintptr_t a, uintptr_t b) { unimpl(n, a, b); return 0; }
#include "jni_stubs.inc"
#undef JNI_STUB

static void *jni_table[NSLOTS] = {
#define JNI_STUB(n) (void *)jni_s##n,
#include "jni_stubs.inc"
#undef JNI_STUB
};
static void *jni_table_ptr = jni_table;

/* implemented slots */
static uintptr_t GetVersion(void *env) { return 0x00010006; }
static void *FindClass(void *env, const char *name) { logf_("JNI FindClass %s\n", name); return new_id("class", name, NULL); }
static void *NewGlobalRef(void *env, void *o) { return o; }
static void DeleteRef(void *env, void *o) { }
static void *GetMethodID(void *env, void *cls, const char *n, const char *s) { logf_("JNI GetMethodID %s %s\n", n, s); return new_id("method", n, s); }
static void *GetStaticMethodID(void *env, void *cls, const char *n, const char *s) { logf_("JNI GetStaticMethodID %s %s\n", n, s); return new_id("static", n, s); }
static void *GetFieldID(void *env, void *cls, const char *n, const char *s) { logf_("JNI GetFieldID %s %s\n", n, s); return new_id("field", n, s); }
static void *GetStaticFieldID(void *env, void *cls, const char *n, const char *s) { logf_("JNI GetStaticFieldID %s %s\n", n, s); return new_id("sfield", n, s); }
static void *NewStringUTF(void *env, const char *s) { return strdup(s ? s : ""); }
static const char *GetStringUTFChars(void *env, void *s, uint8_t *copy) { if (copy) *copy = 0; return (const char *)s; }
static void ReleaseStringUTFChars(void *env, void *s, const char *c) { }
static uintptr_t ExceptionCheck(void *env) { return 0; }
typedef struct { const char *name; const char *sig; void *fn; } JNINativeMethod;
static int RegisterNatives(void *env, void *cls, const JNINativeMethod *m, int n) {
  logf_("JNI RegisterNatives (%d) on %s\n", n, id_name(cls) ? id_name(cls) : "?");
  for (int i = 0; i < n; i++) logf_("   native %s %s -> %p\n", m[i].name, m[i].sig, m[i].fn);
  return 0;
}

/* JavaVM */
static void *jvm_table[8];
static void *jvm_ptr = jvm_table;
static int GetEnv(void *vm, void **penv, int ver) { *penv = &jni_table_ptr; return 0; }
static int AttachCurrentThread(void *vm, void **penv, void *args) { *penv = &jni_table_ptr; return 0; }
static int DetachCurrentThread(void *vm) { return 0; }

static int inited;
static void init(void) {
  if (inited) return; inited = 1;
  jni_table[4] = GetVersion; jni_table[6] = FindClass;
  jni_table[21] = NewGlobalRef; jni_table[22] = DeleteRef; jni_table[23] = DeleteRef;
  jni_table[33] = GetMethodID; jni_table[94] = GetFieldID; jni_table[113] = GetStaticMethodID; jni_table[144] = GetStaticFieldID;
  jni_table[167] = NewStringUTF; jni_table[169] = GetStringUTFChars; jni_table[170] = ReleaseStringUTFChars;
  jni_table[215] = RegisterNatives; jni_table[228] = ExceptionCheck;
  jvm_table[4] = AttachCurrentThread; jvm_table[5] = DetachCurrentThread; jvm_table[6] = GetEnv;
}
void *jni_get_vm(void) { init(); return &jvm_ptr; }
void *jni_get_env(void) { init(); return &jni_table_ptr; }
