// DRAWLIB - off-screen double buffered drawing library for (S)VGA
#ifndef _DRAWLIB_
#define _DRAWLIB_

#define ROW_LEN dscreenwidth()
#define NUM_ROW dscreenheight()
extern int dscreenwidth(void);
extern int dscreenheight(void);
extern int dline(int sx, int sy, int ex, int ey, int pix);
extern int dpolyline(int n, int *xy, int x, int y, int pix);
extern void dfloodfill(int x, int y, int pix, int fill);
extern int dpolyfill(int n, int *xy, int x, int y, int pix, int fill);
extern int dimagesize(int w, int h);
extern int dimageget(int x, int y, int w, int h, void *b, int l);
extern int dimagegetpixel(void *b, int l, int x, int y);
extern int dimageput(int x, int y, int ix, int iy, int iw, int ih, void *b, int l, int t);
extern void drefresh(int index);
#define dimagewidth(img) (((int*)img)[0])
#define dimageheight(img) (((int*)img)[1])
typedef enum {
    DMODE_HIGHEST,
    DMODE_320x200,
    DMODE_640x480,
    DMODE_800x600,
	DMODE_1024x768
} dmode_t;
extern int videomode(dmode_t mode);
extern void restoremode();
extern void *load_bitmap(const char *file, int *blen, int dopal);
extern void waitV(void);

#endif
