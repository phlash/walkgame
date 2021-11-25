// VESA BIOS Extension (VBE) types & functions

#ifndef _VESA_H
#define _VESA_H

#include <stdint.h>

#pragma pack(push,1)
typedef struct {
    char sig[4];
    uint16_t vers;
    uint16_t oemp[2];
    uint32_t caps;
    uint16_t modp[2];
    uint16_t blks;
    uint16_t oemv;
    uint16_t venp[2];
    uint16_t prdp[2];
    uint16_t revp[2];
    uint16_t afvs;
    uint16_t afmp[2];
    uint8_t pad1[216];
    uint8_t pad2[256];
} vbe_info_t;
#define VBE_CAPS_AF	0x08

typedef struct {
    uint16_t matt;
    uint8_t wata;
    uint8_t watb;
    uint16_t gran;
    uint16_t size;
    uint16_t sega;
    uint16_t segb;
    uint16_t posp[2];
    uint16_t bpsl;
    uint16_t widt;
    uint16_t high;
    uint8_t cwid;
    uint8_t chgh;
    uint8_t npln;
    uint8_t bppx;
    uint8_t nbnk;
    uint8_t mmod;
    uint8_t bsiz;
    uint8_t npag;
    uint8_t pad1;
    uint8_t rmsk;
    uint8_t rfld;
    uint8_t gmsk;
    uint8_t gfld;
    uint8_t bmsk;
    uint8_t bfld;
    uint8_t pad2;
    uint8_t pad3;
    uint8_t dcol;
    uint32_t lfbp;
    uint32_t osmp;
    uint16_t osms;
    uint8_t pad4[206];
} vbe_mode_t;
#define VBE_MATT_LFB	0x80
#pragma pack(pop)

extern uint32_t vfindmode(int w, int h, int bpp, uint16_t *pmode, void (*log)(uint16_t, vbe_mode_t *));
extern int vsetmode(uint16_t mode, uint32_t lfbp);
extern void vresetmode(void);

// Request access to LFB when setting a mode
#define VBE_MODE_REQ_LFB	0x4000

#endif
