#ifndef JNI_FAKE_H
#define JNI_FAKE_H
void *jni_get_vm(void);   /* fake JavaVM* */
void *jni_get_env(void);  /* fake JNIEnv* */
#endif
