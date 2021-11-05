// DRAWLIB - off-screen double buffered drawing library for VGA MODE 13h
#ifndef _DRAWLIB_
#define _DRAWLIB_

#define ROW_LEN 320
#define NUM_ROW 200
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
extern int videomode();
extern void restoremode();
extern void *load_bitmap(const char *file, int *blen, int dopal);
extern void waitV(void);

#endif