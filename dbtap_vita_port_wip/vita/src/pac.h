#ifndef PAC_H
#define PAC_H
#include <stdint.h>
#include <stddef.h>

/* .pac container used by Dragon Ball Tap Battle (reverse-engineered):
 *   u16 count; count * { u32 offset; u32 size; char type[8] }; data...
 *   offsets are relative to (2 + 16*count). All little endian. */
typedef struct {
    char type[9];            /* "png","wav","cnv","dac","act","bin","spr","plt","u","ogg" */
    uint32_t size;
    const uint8_t *data;     /* points into Pac.buf */
} PacEntry;

typedef struct {
    uint8_t *buf;
    size_t len;
    uint16_t count;
    PacEntry *entries;
} Pac;

int  pac_load(Pac *p, const char *path);           /* 0 = ok */
void pac_free(Pac *p);
const PacEntry *pac_find(const Pac *p, const char *type, int nth);
#endif
