#include "pac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1]<<8 | p[2]<<16 | (uint32_t)p[3]<<24; }

int pac_load(Pac *p, const char *path) {
    memset(p, 0, sizeof *p);
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 2) { fclose(f); return -2; }
    p->buf = malloc(n);
    if (!p->buf || fread(p->buf, 1, n, f) != (size_t)n) { fclose(f); pac_free(p); return -3; }
    fclose(f);
    p->len = n;
    p->count = p->buf[0] | p->buf[1] << 8;
    size_t base = 2 + 16 * (size_t)p->count;
    if (base > p->len) { pac_free(p); return -4; }
    p->entries = calloc(p->count ? p->count : 1, sizeof(PacEntry));
    for (int i = 0; i < p->count; i++) {
        const uint8_t *e = p->buf + 2 + 16 * i;
        uint32_t off = rd32(e), size = rd32(e + 4);
        if ((uint64_t)base + off + size > p->len) { pac_free(p); return -5; }
        memcpy(p->entries[i].type, e + 8, 8); p->entries[i].type[8] = 0;
        p->entries[i].size = size;
        p->entries[i].data = p->buf + base + off;
    }
    return 0;
}

void pac_free(Pac *p) { free(p->entries); free(p->buf); memset(p, 0, sizeof *p); }

const PacEntry *pac_find(const Pac *p, const char *type, int nth) {
    for (int i = 0; i < p->count; i++)
        if (!strcmp(p->entries[i].type, type) && nth-- == 0) return &p->entries[i];
    return NULL;
}
