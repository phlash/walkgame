#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <vesa.h>
#include <go32.h>

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

    // Process args
    for (int a=1; a<argc; a++) {
        if (strncmp(argv[a],"-?",2)==0)
            return printf("usage: %s [-?] [-w <width:%d>] [-h <height:%d>] [-b <bits/pixel:%d>]\n",
                argv[0], rw, rh, bpp);
        else if (strncmp(argv[a],"-w",2)==0)
            sscanf(argv[++a], "%i", &rw);
        else if (strncmp(argv[a],"-h",2)==0)
            sscanf(argv[++a], "%i", &rh);
        else if (strncmp(argv[a],"-b",2)==0)
            sscanf(argv[++a], "%i", &bpp);
    }
    // Find a mode..
    uint16_t mode;
    uint32_t lfbp = vfindmode(rw, rh, bpp, &mode, vlog);
    if (!lfbp) {
        fprintf(stderr, "unable to find a mode for %dx%dx%d\n", rw, rh, bpp);
        return 1;
    }
    printf("found mode: 0x%04x@0x%08x\n", mode, lfbp);
    getch();
    // Flip it
    uint16_t sel = vsetmode(mode, lfbp);
    if (!sel) {
        fprintf(stderr, "unable to set video mode :(\n");
        return 2;
    }
    // Poke it!
    for (int y=0; y<rh; y++) {
        for (int x=0; x<rw; x++) {
            uint32_t pix = (y<256) ? x&0xff : (y<512) ? (x&0xff)<<8 : (x&0xff)<<16;
            //pix |= 0xff000000;
            movedata(_my_ds(), (unsigned)&pix, sel, (y*rw+x)*sizeof(uint32_t), sizeof(uint32_t));
        }
    }
    getch();
    vresetmode();
    return 0;
}