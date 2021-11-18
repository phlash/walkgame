// Playing with SVGA linear frame buffers
// inspiration from:
// http://www.delorie.com/djgpp/doc/ug/graphics/vbe20.html
// http://www.neuraldk.org/document.php?djgppGraphics
#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <malloc.h>
#include <time.h>
#include <go32.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include "vesa.h"

static vbe_info_t s_vbe;
static int vloadinfo(void) {
	if (s_vbe.vers)
		return 0;
	// detect VBE and read version, mode list etc
	// we use the djgpp transfer buffer..
	__dpmi_regs regs;
	regs.x.ax = 0x4f00;
	regs.x.di = __tb & 0xf;
	regs.x.es = (__tb >> 4) & 0xffff;
	strncpy(s_vbe.sig, "VBE2", 4);
	dosmemput(&s_vbe, sizeof(s_vbe), __tb);
	__dpmi_int(0x10, &regs);
	dosmemget(__tb, sizeof(s_vbe), &s_vbe);
	if (strncmp(s_vbe.sig, "VESA", 4)!=0) {
		fputs("No VBE available", stderr);
		return -1;
	}
	if (s_vbe.vers < 0x0200) {
		fputs("VBE v1.x only", stderr);
		return -2;
	}
	return 0;
}

uint32_t vfindmode(int rw, int rh, uint16_t *pmode) {
	// ensure VBE info loaded
	if (vloadinfo()<0)
		return 0;
	// iterate modes until we find a match
	for (int i=0;; i++) {
		// fetch next mode number from VESA array
		uint16_t mode;
		int off = s_vbe.modp[0]+(s_vbe.modp[1]<<4);
		dosmemget(off + i*sizeof(mode), sizeof(mode), &mode);
		if (0xffff==mode)
			break;
		vbe_mode_t info;
		__dpmi_regs regs;
		regs.x.ax = 0x4f01;
		regs.x.cx = mode;
		regs.x.di = __tb & 0xf;
		regs.x.es = (__tb >> 4) & 0xffff;
		__dpmi_int(0x10, &regs);
		dosmemget(__tb, sizeof(info), &info);
		// compare resolution, ensure 256 colour and LFB available
		if (info.widt==rw &&
			info.high==rh &&
			info.bppx==8 &&
			(info.matt & VBE_MATT_LFB)) {
			*pmode = mode;
			return info.lfbp;
		}
	}
	return 0;
}

static __dpmi_meminfo s_lfbmem;
static int s_lfbsel;
int vsetmode(uint16_t mode, uint32_t lfbp) {
	// map physical address of LFB into our address space
	s_lfbmem.address = lfbp;
	s_lfbmem.size = (uint32_t)s_vbe.blks << 16;
	if (__dpmi_physical_address_mapping(&s_lfbmem)<0) {
		fputs("Failed to map LFB into address space", stderr);
		return 0;
	}
	s_lfbsel = __dpmi_allocate_ldt_descriptors(1);
	if (s_lfbsel<0) {
		fputs("Failed to allocate a selector for LFB", stderr);
		__dpmi_free_physical_address_mapping(&s_lfbmem);
		return 0;
	}
	__dpmi_set_segment_base_address(s_lfbsel, s_lfbmem.address);
	__dpmi_set_segment_limit(s_lfbsel, s_lfbmem.size-1);
	// change mode - including LFB request bit
	__dpmi_regs regs;
	regs.x.ax = 0x4f02;
	regs.x.bx = mode | VBE_MODE_REQ_LFB;
	__dpmi_int(0x10, &regs);
	return s_lfbsel;
}

void vresetmode(void) {
	if (s_lfbsel>0) {
		__dpmi_regs regs;
		regs.x.ax = 3;
		__dpmi_int(0x10, &regs);
		__dpmi_free_ldt_descriptor(s_lfbsel);
		__dpmi_free_physical_address_mapping(&s_lfbmem);
		s_lfbsel = 0;
	}
}

