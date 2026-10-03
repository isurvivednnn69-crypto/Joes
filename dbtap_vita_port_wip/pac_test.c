#include "vita/src/pac.h"
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    long total = 0; int bad = 0;
    for (int i = 1; i < argc; i++) {
        Pac p; int r = pac_load(&p, argv[i]);
        if (r) { printf("FAIL %s (%d)\n", argv[i], r); bad++; continue; }
        total += p.count;
        for (int k = 0; k < p.count; k++) {
            if (!strcmp(p.entries[k].type,"png") && (p.entries[k].size<8 || p.entries[k].data[1]!='P')) { printf("bad png %s #%d\n", argv[i], k); bad++; }
            else if (!strcmp(p.entries[k].type,"wav") && (p.entries[k].size<12 || p.entries[k].data[0]!='R')) { printf("bad wav %s #%d\n", argv[i], k); bad++; }
        }
        pac_free(&p);
    }
    printf("files=%d entries=%ld problems=%d\n", argc-1, total, bad);
    return bad != 0;
}
