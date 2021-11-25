#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <conio.h>
#include <vesa.h>
#include <go32.h>
#define QOI_IMPLEMENTATION
#include <qoi.h>

void vlog(uint16_t m, vbe_mode_t *info) {
    printf("  mode: 0x%04x w=%d h=%d bpp=%d mod=0x%02x col=%02x lfbp=0x%08x\n",
        m, info->widt, info->high, info->bppx, info->mmod,
        info->dcol,
        info->lfbp);
}
int main(int argc, char **argv) {
    int rw = 800;
    int rh = 600;
    int bpp = 32;
    char *img = "gfx/apple.qoi";

    // Process args
    for (int a=1; a<argc; a++) {
        if (strncmp(argv[a],"-?",2)==0)
            return printf("usage: %s [-?] [-w <width:%d>] [-h <height:%d>] [-b <bits/pixel:%d>] [-i <image:%s>]\n",
                argv[0], rw, rh, bpp, img);
        else if (strncmp(argv[a],"-w",2)==0)
            sscanf(argv[++a], "%i", &rw);
        else if (strncmp(argv[a],"-h",2)==0)
            sscanf(argv[++a], "%i", &rh);
        else if (strncmp(argv[a],"-b",2)==0)
            sscanf(argv[++a], "%i", &bpp);
        else if (strncmp(argv[a],"-i",2)==0)
            img = argv[++a];
    }
    // Find a mode..
    uint16_t mode;
    uint32_t lfbp = vfindmode(rw, rh, bpp, &mode, vlog);
    if (!lfbp) {
        fprintf(stderr, "unable to find a mode for %dx%dx%d\n", rw, rh, bpp);
        return 1;
    }
    // Allocate a buffer
    uint32_t *buf = calloc(rw*rh, sizeof(uint32_t));
    if (!buf) {
        fprintf(stderr, "unable to allocate off-screen buffer :(\n");
        return 1;
    }
    // Load test image
    int iw, ih;
    uint32_t *test = qoi_read(img, &iw, &ih, 4);
    if (!img)
        fprintf(stderr, "unable to load: %s\n", img);
    printf("found mode: 0x%04x@0x%08x\n", mode, lfbp);
    getch();
    // Flip it
    uint16_t sel = vsetmode(mode, lfbp);
    if (!sel) {
        fprintf(stderr, "unable to set video mode :(\n");
        return 2;
    }
    // Poke it!
    if (test) {
        for (int y=0; y<rh-ih; y+=ih)
            for (int x=0; x<rw-iw; x+=iw)
                for (int iy=0; iy<ih; iy++)
                    for (int ix=0; ix<iw; ix++)
                        buf[(y+iy)*rw+x+ix] = test[iy*iw+ix];
    } else {
        for (int y=0; y<rh; y++) {
            for (int x=0; x<rw; x++) {
                uint32_t pix = (y<256) ? x&0xff : (y<512) ? (x&0xff)<<8 : (x&0xff)<<16;
                buf[y*rw+x] = pix;
            }
        }
    }
    clock_t beg = clock();
    uint32_t frms = 0;
    while (!kbhit()) {
        // flip some bits each frame..
        uint32_t off = frms%(rh-10);
        for (int y=0; y<10; y++)
            for (int x=0; x<10; x++)
                buf[(y+off)*rw+x+off] ^= 0x00ffffff;
        movedata(_my_ds(), (unsigned)buf, sel, 0, rw*rh*sizeof(uint32_t));
        frms+=1;
    }
    clock_t end = clock();
    getch();
    vresetmode();
    printf("FPS: %.2f\n", (double)frms*(double)CLOCKS_PER_SEC/(double)(end-beg));
    return 0;
}