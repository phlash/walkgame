#include <stdio.h>
#include <dos.h>
#include <string.h>
#include <malloc.h>
#include <drawlib.h>
#ifdef DJGPP
#include <vesa.h>
#include <go32.h>
#include <dpmi.h>
#else
typedef unsigned char uint8_t;
typedef unsigned int uint16_t;
typedef unsigned long uint32_t;
#endif

// ------ DRAWING LIBRARY CODE --------

// video memory (64kB window)
#ifdef DJGPP
// for DJGPP, we use the physical offset from _dos_ds, or a special selector
static uint32_t voff = 0xa0000;
static int vsel = 0;
#else
// for TurboC we construct a far pointer
static uint8_t *vmem = MK_FP(0xa000,0);
#endif
// drawing buffer
static uint8_t *dmem;
// current graphics mode
static dmode_t mode;

// screen dimension (or -1 if not in graphics mode)
int dscreenwidth(void) {
    return DMODE_320x200==mode ? 320 : DMODE_640x480==mode ? 640 : DMODE_800x600==mode ? 800 : DMODE_1024x768==mode ? 1024 : -1;
}
int dscreenheight(void) {
    return DMODE_320x200==mode ? 200 : DMODE_640x480==mode ? 480 : DMODE_800x600==mode ? 600 : DMODE_1024x768==mode ? 768 : -1;
}

int dline(int sx, int sy, int ex, int ey, int pix) {
    // naive (unclipped) line
    int dx = ex-sx;
    int dy = ey-sy;
    if (sx<0 || sx>ROW_LEN ||
        ex<0 || ex>ROW_LEN ||
        sy<0 || sy>NUM_ROW ||
        ey<0 || ey>NUM_ROW)
        return -1;
    dx = (dx<0) ? -dx : dx;
    dy = (dy<0) ? -dy : dy;
    if (0==dx) {
        // special case, only y co-ord changes
        int y = sy;
        dy = (ey<sy) ? -1 : 1;
        do {
            dmem[y*ROW_LEN+sx] = (uint8_t)pix;
            if (y!=ey)  // special case of ey==sy
                y += dy;
        } while (y!=ey);
    } else if (0==dy) {
        // special case, only x co-ord changes
        int x = sx;
        dx = (ex<sx) ? -1: 1;
        do {
            dmem[sy*ROW_LEN+x] = (uint8_t)pix;
            if (x!=ex)
                x += dx;
        } while (x!=ex);
    } else if (dy>dx) {
        // slope more vertical than horizontal, iterate y
        int x = sx, y = sy, p = 0, l = dy;
        dy = (ey<sy) ? -1 : 1;
        dx = ex-sx;
        do {
            dmem[y*ROW_LEN+x] = (uint8_t)pix;
            if (p!=l) {
                p += 1;
                y += dy;
                x = sx + (int)(((long)p*(long)dx)/(long)l);
            }
        } while (p!=l);
    } else {
        // slope more horizontal than vertical, iterate x
        int x = sx, y = sy, p = 0, l = dx;
        dx = (ex<sx) ? -1 : 1;
        dy = ey-sy;
        do {
            dmem[y*ROW_LEN+x] = (uint8_t)pix;
            if (p!=l) {
                p += 1;
                x += dx;
                y = sy + (int)(((long)p*(long)dy)/(long)l);
            }
        } while (p!=l);
    }
    return 0;
}
int dpolyline(int n, int *xy, int x, int y, int pix) {
    // multi-line polygon
    int p;
    for (p=0; p<2*n-2; p+=2) {
        if (dline(x+xy[p], y+xy[p+1], x+xy[p+2], y+xy[p+3], pix))
            return -1;
    }
    return 0;
}
// internal structure for floodfill data
typedef struct { int minx, miny, maxx, maxy; int pix, fill; } _dflood_t;
int _dfillrow(_dflood_t *pf, int x, int y, int dy, int *sx, int nx) {
    // internal function to flood one row
    int b;
    // scan left until border found or we hit minx
    while (x>pf->minx && dmem[y*ROW_LEN+x]!=pf->pix)
        x-=1;
    // scan right until border found or we hit maxx, filling pixels
    // and recording seed points in adjacent row (according to dy)
    x+=1;
    b = 1;
    while (x<pf->maxx && dmem[y*ROW_LEN+x]!=pf->pix) {
        dmem[y*ROW_LEN+x] = (uint8_t)pf->fill;
        if ((y+dy)>=pf->miny && (y+dy)<pf->maxy) {
            if (dmem[(y+dy)*ROW_LEN+x]!=pf->pix) {
                if (b)
                    sx[nx++] = x;
                b = 0;
            } else {
                b = 1;
            }
        }
        x+=1;
    }
    return nx;
}
void _dfloodfill(_dflood_t *pf, int x, int y) {
    // internal row-based floodfill
    int sy, nx, dx, wx;
    int *sx[2];
    sx[0] = malloc(ROW_LEN/2*sizeof(int));
    sx[1] = malloc(ROW_LEN/2*sizeof(int));
    // fill seed row, find seed points in previous row
    sy = y;
    dx = 1;
    wx = 0;
    sx[0][0] = x;
    while (dx) {
        int i;
        for (i=0, nx=0; i<dx; i++)
            nx = _dfillrow(pf, sx[wx][i], sy, -1, sx[1-wx], nx);
        sy -= 1;
        wx = 1-wx;
        dx = nx;
    }
    // fill seed row (again), find seed points in following row
    sy = y;
    dx = 1;
    wx = 0;
    sx[0][0] = x;
    while (dx) {
        int i;
        for (i=0, nx=0; i<dx; i++)
            nx = _dfillrow(pf, sx[wx][i], sy, 1, sx[1-wx], nx);
        sy += 1;
        wx = 1-wx;
        dx = nx;
    }
    free(sx[0]);
    free(sx[1]);
}
void dfloodfill(int x, int y, int pix, int fill) {
    // public floodfill (extent == whole screen)
    _dflood_t f;
    f.minx = f.miny = 0;
    f.maxx = ROW_LEN;
    f.maxy = NUM_ROW;
    f.pix = pix;
    f.fill = fill;
    _dfloodfill(&f, x, y);
}
int dpolyfill(int n, int *xy, int x, int y, int pix, int fill) {
    // filled multi-line polygon
    _dflood_t f;
    int p;
    f.minx=x+xy[0];
    f.miny=y+xy[1];
    f.maxx=x+xy[0];
    f.maxy=y+xy[1];
    f.pix=pix;
    f.fill=fill;
    for (p=0; p<2*n-2; p+=2) {
        int sx=x+xy[p], sy=y+xy[p+1], ex=x+xy[p+2], ey=y+xy[p+3];
        if (dline(sx, sy, ex, ey, pix))
            return -1;
        f.minx = ex<f.minx ? ex : f.minx;
        f.miny = ey<f.miny ? ey : f.miny;
        f.maxx = ex>f.maxx ? ex : f.maxx;
        f.maxy = ey>f.maxy ? ey : f.maxy;
    }
    // floodfill - assumes centre of extent is a valid seed point
    _dfloodfill(&f, (f.minx+f.maxx)/2, (f.miny+f.maxy)/2);
    return 0;
}
int dimagesize(int w, int h) {
    // buffer size required to grab image
    return w*h+2*sizeof(int);
}
int dimageget(int x, int y, int w, int h, void *b, int l) {
    // grab image into buffer
    int row;
    uint8_t *f = (uint8_t *)b;
    if (w<0 || h<0 || w>ROW_LEN || h>NUM_ROW)
        return -1;  // invalid size
    if (y<0 || y+h>NUM_ROW || x<0 || x+w>ROW_LEN)
        return -2;  // out of bounds
    if (l<dimagesize(w, h))
        return -3;  // buffer too small
    *(int *)f = w;
    f += sizeof(int);
    *(int *)f = h;
    f += sizeof(int);
    for (row=y; row<y+h; row++) {
        memcpy(f, dmem+row*ROW_LEN+x, w);
        f += w;
    }
    return dimagesize(w, h);
}
int dimagegetpixel(void *b, int l, int x, int y){
    uint8_t *f = (uint8_t *)b;
    int w, h, row;
    w = *(int *)f;
    f += sizeof(int);
    h = *(int *)f;
    f += sizeof(int);
    if (w<0 || h<0 || dimagesize(w, h)>l)
        return -1;  // buffer invalid
    if (x<0 || x>w || y<0 || y>h){
        return -2;  // out-of-bounds
    }
    return f[y*w + x];
}
int dimageput(int x, int y, int sx, int sy, int sw, int sh, void *b, int l, int t) {
    // draw image from buffer, s[x,y,w,h] = sub-section, t=transparent pixel (or -1)
    uint8_t *f = (uint8_t *)b;
    int w, h, row;
    w = *(int *)f;
    f += sizeof(int);
    h = *(int *)f;
    f += sizeof(int);
    if (w<0  || h<0  || dimagesize(w, h)>l)
        return -1;  // buffer invalid
    // zero sub-section size => full image
    sw = (sw) ? sw : w;
    sh = (sh) ? sh : h;
    if (sx<0 || sx>w || sy<0 || sy>h || sw<0 || sx+sw>w || sh<0 || sy+sh>h)
        return -3;  // sub-section out of bounds
    f += sy*w;
    if (x<0 || x+sw>ROW_LEN || y<0 || y+sh>NUM_ROW)
        return -2;  // out of bounds
    for (row=y; row<y+sh; row++) {
        if (t<0) {
            memcpy(dmem+row*ROW_LEN+x, f+sx, sw);
        } else {
            int col;
            for (col=0; col<sw; col++) {
                if (f[col+sx]!=t)
                    dmem[row*ROW_LEN+col+x] = f[col+sx];
            }
        }
        f += w;
    }
    return dimagesize(w, h);
}
void drefresh(int index) {
    // refresh the video display, clear the draw buffer
    if (DMODE_320x200==mode) {
#ifdef DJGPP
		movedata(_my_ds(), (unsigned)dmem, _dos_ds, voff, ROW_LEN*NUM_ROW);
#else
		memcpy(vmem, dmem, ROW_LEN*NUM_ROW);
#endif
	} else {
#ifdef DJGPP
		movedata(_my_ds(), (unsigned)dmem, vsel, 0, ROW_LEN*NUM_ROW);
#else
		// unsupported :=(
#endif
	}
	memset(dmem, index, ROW_LEN*NUM_ROW);
}

int videomode(dmode_t req) {
    // anything other than VGA 320x200, only under DJGPP & requires VBE
    if (DMODE_320x200!=req) {
#ifdef DJGPP
        int test = (int)DMODE_1024x768 + 1;
        do {
            uint16_t vmode;
			uint32_t lfbp;
            int rw, rh;
            test = DMODE_HIGHEST==req ? test-1 : (int)req;
            rw = DMODE_640x480==test ? 640 : DMODE_800x600==test ? 800 : 1024;
            rh = DMODE_640x480==test ? 480 : DMODE_800x600==test ? 600 : 768;
            lfbp = vfindmode(rw, rh, 8, &vmode);
            if (lfbp>0) {
                mode = (dmode_t)test;
                vsel = vsetmode(vmode, lfbp);
				if (vsel<0) {
					fputs("Unable to set SVGA video mode", stderr);
					return -1;
				}
            }
        } while (DMODE_HIGHEST==mode && DMODE_HIGHEST==req && test>DMODE_640x480);
#else
		fputs("SVGA not supported under TurboC, using 320x200\n", stderr);
		req = DMODE_320x200;
#endif
    }
    // if we didn't find a working VBE mode, or we were asked for VGA 320x200
    if (DMODE_HIGHEST==mode && (DMODE_320x200==req || DMODE_HIGHEST==req)) {
        // set requested video mode
#ifdef DJGPP
		__dpmi_regs regs;
		regs.x.ax = 0x13;
		__dpmi_int(0x10, &regs);
#else
        union REGS regs;
        regs.h.ah = 0x00;
        regs.h.al = 0x13;       // ye infamous MODE 13h (320x200x256)
        int86(0x10, &regs, &regs);
#endif
        mode = DMODE_320x200;
    }
    // no mode set? bail.
    if (DMODE_HIGHEST==mode)
        return -1;
    // allocate a drawing buffer
#ifdef DJGPP
    dmem = calloc(dscreenheight(), dscreenwidth());
#else
    dmem = _fcalloc(dscreenheight(), dscreenwidth());
#endif
    if (!dmem) {
        // oops - no memory left
        fputs("out of memory :=(", stderr);
        restoremode();
        return -1;
    }
    return 0;
}
void restoremode() {
    // done!
    if (mode>DMODE_HIGHEST) {
#ifdef DJGPP
		__dpmi_regs regs;
		regs.x.ax = 3;
		__dpmi_int(0x10, &regs);
		if (dmem)
			free(dmem);
#else
        union REGS regs;
        regs.h.ah = 0x00;
        regs.h.al = 3;
        int86(0x10, &regs, &regs);
        if (dmem) {
            _ffree(dmem);
        }
#endif
        mode = DMODE_HIGHEST;
    }
}

// ------ UTILITY CODE -------
void set_bmp_palette(uint8_t *pal, int ncol) {
    // as read from BMP file, in BGRA tuples
    int c;
    outportb(0x3c8, 0);
    for (c=0; c<ncol; c++) {
        outportb(0x3c9, pal[4*c+2]>>2);
        outportb(0x3c9, pal[4*c+1]>>2);
        outportb(0x3c9, pal[4*c+0]>>2);
    }
}
void set_vga_palette(uint8_t *pal, int ncol) {
    // as expected by the VGA, in RGB tuples
    int c;
    outportb(0x3c8, 0);
    for (c=0; c<ncol; c++) {
        outportb(0x3c9, pal[3*c+0]);
        outportb(0x3c9, pal[3*c+1]);
        outportb(0x3c9, pal[3*c+2]);
    }
}
#pragma pack(push,1)
typedef struct {
    // bitmap file header
    uint16_t bmid;
    uint32_t fsiz;
    uint32_t resv;
    uint32_t bits;
    // bitmap info header (DIB)
    uint32_t hsiz;
    uint32_t cols;
    uint32_t rows;
    uint16_t plns;
    uint16_t bppx;
    uint32_t comp;
    uint32_t isiz;
    uint32_t hrez;
    uint32_t vrez;
    uint32_t npal;
    uint32_t nimp;
} bitmap_load_t;
#pragma pack(pop)
void *load_bitmap(const char *file, int *blen, int dopal) {
    // BMP file loader, also adjusts VGA palette
    bitmap_load_t bmp;
    uint8_t *img = NULL, *pal, *tmp;
    uint32_t row;
    FILE *fp = fopen(file, "rb");
    if (!fp)
        goto out;
    if (fread(&bmp, sizeof(bmp), 1, fp)!=1)
        goto out;
    // check we have a valid, usable BMP
    if (bmp.bmid!=0x4d42 ||
        bmp.hsiz<40 ||
        bmp.bppx!=8 ||
        bmp.comp!=0)
        goto out;
    // seek past larger headers
    if (bmp.hsiz>40 && fseek(fp, bmp.hsiz-40, SEEK_CUR)<0)
        goto out;
    // process palette if requested
    if (dopal) {
        if (!bmp.npal) bmp.npal = 256;
        pal = calloc(bmp.npal, 4);
        if (!pal)
            goto out;
        if (fread(pal, 4, bmp.npal, fp)!=bmp.npal) {
            free(pal);
            goto out;
        }
        set_bmp_palette(pal, bmp.npal);
        free(pal);
    }
    // allocate memory image buffer
    img = calloc(dimagesize(bmp.cols, bmp.rows),1);
    if (!img)
        goto out;
    // fill in details
    ((int *)img)[0] = bmp.cols;
    ((int *)img)[1] = bmp.rows;
    if (fseek(fp, bmp.bits, SEEK_SET)<0 ||
        fread(img+2*sizeof(int), bmp.rows*bmp.cols, 1, fp)!=1) {
        free(img);
        img=NULL;
        goto out;
    }
    // reverse rows (thanks Microsoft!)
    tmp = malloc(bmp.cols);
    for (row=0; tmp && row<(bmp.rows+1)/2; row++) {
        int o1 = 2*sizeof(int)+row*bmp.cols;
        int o2 = 2*sizeof(int)+(bmp.rows-row-1)*bmp.cols;
        memcpy(tmp, img+o1, bmp.cols);
        memcpy(img+o1, img+o2, bmp.cols);
        memcpy(img+o2, tmp, bmp.cols);
    }
    if (tmp) free(tmp);
    // record buffer length
    *blen = dimagesize(bmp.cols, bmp.rows);
out:
    if (fp) fclose(fp);
    return img;
}
void waitV() {
    // wait if the vsync bit is already up (in sync)
    while((inportb(0x3da) & 0x08));
    // ..then wait for it to go up again.
    while(!(inportb(0x3da) & 0x08));
}
