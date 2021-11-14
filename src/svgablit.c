#include <alloc.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <conio.h>
#include <time.h>
#include <vesa.h>

#define XRES    800L
#define YRES    600L


static vbe_mode_t minf;
uint16_t showmode(uint16_t mode, vbe_access_t *pacc) {
    union REGS regs;
    struct SREGS sregs;
    regs.x.ax = 0x4f01;
    regs.x.cx = mode;
    regs.x.di = FP_OFF(&minf);
    sregs.es = FP_SEG(&minf);
    int86x(0x10, &regs, &regs, &sregs);
    if (regs.x.ax!=0x004f) {
        printf("oops, mode info request failed: %d\n", regs.h.ah);
        return 0;
    }
    printf("    attr:0x%04x wata:0x%02x watb:0x%02x gran:%dk size:%dk sega:0x%04x segb:0x%04x\n"
           "    bpsl:%d WxHxD:%dx%dx%d mmod:0x%02x bsiz:%dk npag:%d lfbp=0x%08x\n",
        minf.matt, minf.wata, minf.watb, minf.gran, minf.size, minf.sega, minf.segb, minf.bpsl,
        minf.widt, minf.high, minf.bppx, minf.mmod, minf.bsiz, minf.npag+1, minf.lfbp);
    // suitable mode?
    if ((minf.matt&0x41)==0x01 &&       // supported, bank switchable
        minf.mmod==0x04 &&              // packed pixel model
        minf.wata==0x07 &&              // read/write window exists
        minf.sega!=0 &&                 // segment available
        minf.bpsl==minf.widt &&         // no padding on lines
        minf.widt==XRES && minf.high==YRES && minf.bppx==8) { // XxYx256
        // fill out access data
        if (pacc) {
            pacc->win = MK_FP(minf.sega, 0);
            pacc->gran = minf.gran;
            pacc->size = minf.size;
            return mode;
        }
    }
    return 0;
}
void bitblat(uint8_t huge *dbuf, vbe_access_t *pacc) {
    // Bitblat to video memory through banked area.
    uint16_t b = 0;
    uint32_t xf = 0L;
    uint32_t wsz = (uint32_t)pacc->size*1024L;
    while (xf<(XRES*YRES)) {
        uint32_t rem = XRES*YRES-xf;
        uint32_t cp;
        union REGS regs;
        // Select bank
        regs.x.ax = 0x4f05;
        regs.x.bx = 0;
        regs.x.dx = b;
        int86(0x10, &regs, &regs);
        if (regs.x.ax!=0x004f) {
            break;
        }
        cp = (rem<wsz) ? rem : wsz;
        // always split into two as size_t could overflow with a single call
        cp = cp/2;
        memcpy(pacc->win, &dbuf[xf], (size_t)cp);
        xf += cp;
        memcpy(pacc->win+cp, &dbuf[xf], (size_t)cp);
        xf += cp;
        b += (pacc->size/pacc->gran);
    }
}
static vbe_info_t info;
int main() {
    union REGS regs;
    struct SREGS sregs;
    char *oems;
    uint16_t *mods, chosen, x, y, frm;
    vbe_access_t acc;
    uint8_t oldm;
    uint8_t huge *dbuf = farcalloc(XRES, YRES);
    clock_t beg, end;
    if (!dbuf) {
        printf("oops - unable to allocate draw buffer\n");
        return 1;
    }
    // VESA check..
    strncmp(info.sig, "VBE2", 4);
    regs.x.ax = 0x4f00;
    regs.x.di = FP_OFF(&info);
    sregs.es = FP_SEG(&info);
    int86x(0x10, &regs, &regs, &sregs);
    if (regs.x.ax!=0x004f) {
        printf("sorry, VESA not detected\n");
        return 1;
    }
    oems = MK_FP(info.oemp[1], info.oemp[0]);
    mods = MK_FP(info.modp[1], info.modp[0]);
    printf("VBE info:\n");
    printf("     sign: %.4s\n", info.sig);
    printf("  version: 0x%x\n", info.vers);
    printf("  oem ptr: %Fp (%Fs)\n", oems, oems);
    printf("     caps: 0x%lx\n", info.caps);
    printf("  modeptr: %Fp\n", mods);
    printf("  64k blk: %d\n", info.blks);
    printf("Modes:\n");
    chosen = 0;
    while (*mods!=0xffff) {
        printf("  0x%04x:\n", *mods);
        if (showmode(*mods, chosen ? NULL : &acc) && !chosen)
            chosen = *mods;
        mods++;
    }
    printf("<Esc> to quit or <Return> to enter: 0x%04x (%p/%d/%d)\n", chosen, acc.win, acc.gran, acc.size);
    if (0x1b==getch())
        return 0;
    // grab current mode
    regs.x.ax = 0x0f00;
    int86(0x10, &regs, &regs);
    oldm = regs.h.al;
    // Enter VESA mode!
    regs.x.ax = 0x4f02;
    regs.x.bx = chosen;
    int86(0x10, &regs, &regs);
    if (regs.x.ax!=0x004f) {
        printf("oops, unable to change video mode: %d\n", regs.h.ah);
        return 1;
    }
    // Write pattern to draw buffer..
    for (y=0; y<YRES; y++)
        memset(&dbuf[y*XRES], y%256, XRES);
    // blat speed test
    beg = clock();
    frm = 0;
    while (!kbhit()) {
        for (y=0; y<10; y++)
            for (x=0; x<10; x++) {
                uint16_t o = frm%(YRES-10);
                dbuf[XRES*(y+o)+x+o] ^= 0xff;
            }
        bitblat(dbuf, &acc);
        frm += 1;
    }
    end = clock();
    getch();
    // Return to old mode
    regs.h.ah = 0;
    regs.h.al = oldm;
    int86(0x10, &regs, &regs);
    printf("fps=%.2f\n", (float)(frm*CLK_TCK)/(float)(end-beg));
    farfree(dbuf);
    return 0;
}