#ifndef COMPAT_H
#define COMPAT_H
#include "so_util.h"
#define DATA_PATH "ux0:data/spiderman"
extern const so_import compat_imports[];
extern const size_t compat_imports_count;
void compat_set_module(so_module *m);
uintptr_t unresolved_stub_addr(void);
#endif
