// VESA BIOS Extension (VBE) routines
#include <string.h>
#include <dos.h>
#include <vesa.h>

static vbe_info_t info;
static vbe_mode_t minf;
static int _vgetinfo(void) {
    union REGS regs;
    struct SREGS sregs;
    printf("_vgetinfo\n");
    // already loaded..
    if (info.vers)
        return 0;
    // VESA check..
    printf("_vgetinfo: BIOS call\n");
    strncmp(info.sig, "VBE2", 4);
    regs.x.ax = 0x4f00;
    regs.x.di = FP_OFF(&info);
    sregs.es = FP_SEG(&info);
    int86x(0x10, &regs, &regs, &sregs);
    if (regs.x.ax!=0x004f) {
        // no VBE detected
        printf("_vgetinfo: no VBE\n");
        return -1;
    }
    printf("_vgetinfo: OK\n");
    return 0;
}
uint16_t vfindmode(int rw, int rh, vbe_access_t *pacc) {
    union REGS regs;
    struct SREGS sregs;
    uint16_t *mods;
    if (_vgetinfo()<0)
        return 0;
    // search mode list for matching resolution
    mods = MK_FP(info.modp[1], info.modp[0]);
    while (*mods!=0xffff) {
        printf("_vfindmode: 0x%04x\n", *mods);
        regs.x.ax = 0x4f01;
        regs.x.cx = *mods;
        regs.x.di = FP_OFF(&minf);
        sregs.es = FP_SEG(&minf);
        int86x(0x10, &regs, &regs, &sregs);
        if (regs.x.ax!=0x004f) {
            return 0;
        }
        // suitable mode?
        if ((minf.matt&0x41)==0x01 &&       // supported, bank switchable
            minf.mmod==0x04 &&              // packed pixel model
            minf.wata==0x07 &&              // read/write window exists
            minf.sega!=0 &&                 // segment available
            minf.bpsl==minf.widt &&         // no padding on lines
            minf.widt==rw && minf.high==rh && minf.bppx==8) { // XxYx256
            // fill out access data
            if (pacc) {
                pacc->win = MK_FP(minf.sega, 0);
                pacc->gran = minf.gran;
                pacc->size = minf.size;
                pacc->widt = minf.widt;
                pacc->high = minf.high;
                printf("vfindmode: OK\n");
                return *mods;
            }
        }
        mods++;
    }
    printf("vfindmode: FAIL\n");
    return 0;
}
uint8_t vsetmode(uint16_t m) {
    union REGS regs;
    uint8_t oldm;
    // grab current mode
    regs.x.ax = 0x0f00;
    int86(0x10, &regs, &regs);
    oldm = regs.h.al;
    // Enter VESA mode!
    regs.x.ax = 0x4f02;
    regs.x.bx = m;
    int86(0x10, &regs, &regs);
    if (regs.x.ax!=0x004f) {
        return 0;
    }
    return oldm;
}
void vblit(uint8_t **buf, vbe_access_t *pacc) {
    // Bitblat to video memory through banked area.
    uint16_t b = 0;
    uint32_t xf = 0L;
    uint32_t bl = (uint32_t)pacc->widt * (uint32_t)pacc->high;
    uint32_t wsz = (uint32_t)pacc->size*1024L;
    uint16_t row = 0, rl = 0;
    while (xf<bl) {
        uint32_t rem = bl-xf;
        uint32_t cp;
        uint32_t wo = 0;
        union REGS regs;
        // Select bank
        regs.x.ax = 0x4f05;
        regs.x.bx = 0;
        regs.x.dx = b;
        int86(0x10, &regs, &regs);
        if (regs.x.ax!=0x004f) {
            break;
        }
        // How many bytes are we copying (window size or less)
        cp = (rem<wsz) ? rem : wsz;
        // Copy any remainder of a previous line
        if (rl) {
            memcpy(pacc->win, buf[row]+(pacc->widt-rl), rl);
            rl = 0;
            wo = rl;
            row += 1;
        }
        // Copy buffer lines until we hit limit
        while (wo<cp) {
            if (wo+pacc->widt>cp) {
                // Partial line at the end
                uint16_t cl = cp-wo;
                rl = pacc->widt-cl;
                memcpy(pacc->win+wo, buf[row], cl);
                wo = cp;
                // We do not increment row here
            } else {
                // Full line to window
                memcpy(pacc->win+wo, buf[row], pacc->widt);
                wo += pacc->widt;
                row += 1;
            }
        }
        // Next page to map into window
        b += (pacc->size/pacc->gran);
    }
}