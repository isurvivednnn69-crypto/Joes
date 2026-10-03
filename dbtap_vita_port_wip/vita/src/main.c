/* Step-1 test app: proves the Vita can read the game's .pac files, decode the
 * PNG atlases and draw them. Tap the touchscreen to cycle through textures,
 * press START to quit. UNTESTED on hardware (written without a VitaSDK).
 * Needs stb_image.h (public domain, https://github.com/nothings/stb) in src/. */
#include <vitasdk.h>
#include <vitaGL.h>
#include <stdio.h>
#include <stdlib.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#include "pac.h"

#define ASSET_DIR "ux0:data/dbtap/assets/"   /* copy the APK's assets/*.pac here */

static GLuint upload_png(const PacEntry *e, int *w, int *h) {
    int c; unsigned char *px = stbi_load_from_memory(e->data, e->size, w, h, &c, 4);
    if (!px) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *w, *h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(px);
    return t;
}

static void draw_quad(GLuint tex, float x, float y, float w, float h) {
    const float v[] = { x,y, x+w,y, x,y+h, x+w,y+h };
    const float uv[] = { 0,0, 1,0, 0,1, 1,1 };
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, v); glTexCoordPointer(2, GL_FLOAT, 0, uv);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

int main(void) {
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    vglInit(0x800000);
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Pac pac; int idx = 0, was_down = 0; GLuint tex = 0;
    if (pac_load(&pac, ASSET_DIR "char00.pac") != 0) { sceKernelExitProcess(0); }

    int npng = 0; while (pac_find(&pac, "png", npng)) npng++;
    for (;;) {
        SceCtrlData pad; sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) break;
        SceTouchData t; sceTouchPeek(SCE_TOUCH_PORT_FRONT, &t, 1);
        int down = t.reportNum > 0;
        if (down && !was_down) { idx = (idx + 1) % (npng ? npng : 1); if (tex) glDeleteTextures(1, &tex); tex = 0; }
        was_down = down;
        if (!tex && npng) { int w, h; tex = upload_png(pac_find(&pac, "png", idx), &w, &h); }

        vglStartRendering();
        glClearColor(0.1f, 0.1f, 0.15f, 1); glClear(GL_COLOR_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, 960, 544, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        if (tex) draw_quad(tex, 224, 16, 512, 512);
        vglStopRendering();
    }
    pac_free(&pac);
    vglEnd();
    sceKernelExitProcess(0);
    return 0;
}
