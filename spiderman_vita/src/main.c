/* Amazing Spider-Man (Gameloft, libSpiderMan.so) -> PS Vita loader. MILESTONE 1:
 * load + relocate + resolve, run constructors, call JNI_OnLoad, write everything to ux0:data/spiderman/log.txt */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>
#include <unistd.h>
#include "so_util.h"
#include "compat.h"
#include "jni_fake.h"

#define LOAD_ADDRESS 0x98000000

int _newlib_heap_size_user = 96 * 1024 * 1024;

static FILE *g_log;
void logf_(const char *fmt, ...) {
  va_list ap;
  if (!g_log) g_log = fopen(DATA_PATH "/log.txt", "a");
  if (!g_log) return;
  va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
  fflush(g_log); fsync(fileno(g_log));
}

static so_module mod;

static int game_thread(SceSize args, void *argp) {
  logf_("=== spiderman_vita milestone 1 ===\n");
  if (so_load(&mod, DATA_PATH "/libSpiderMan.so", LOAD_ADDRESS) < 0) goto done;
  compat_set_module(&mod);
  so_link(&mod, compat_imports, compat_imports_count);
  so_protect(&mod);
  chdir(DATA_PATH);

  so_run_init(&mod);

  int (*on_load)(void *, void *) = (int (*)(void *, void *))so_symbol(&mod, "JNI_OnLoad");
  if (on_load) {
    logf_("calling JNI_OnLoad @ %p\n", on_load);
    int v = on_load(jni_get_vm(), NULL);
    logf_("JNI_OnLoad returned 0x%x\n", v);
  } else logf_("JNI_OnLoad not found\n");

done:
  logf_("=== end of milestone 1 run ===\n");
  sceKernelDelayThread(3 * 1000 * 1000);
  sceKernelExitProcess(0);
  return 0;
}

int main(void) {
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
  sceIoMkdir("ux0:data", 0777);
  sceIoMkdir(DATA_PATH, 0777);

  SceUID t = sceKernelCreateThread("game", game_thread, 0x40, 4 * 1024 * 1024, 0, 0, NULL);
  sceKernelStartThread(t, 0, NULL);
  sceKernelWaitThreadEnd(t, NULL, NULL);
  return 0;
}
