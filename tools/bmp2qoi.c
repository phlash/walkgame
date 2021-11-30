/*

Command line tool to convert between bmp <> qoi format

    gcc bmp2qoi.c -std=c99 -O3 -o bmp2qoi

Dominic Szablewski - https://phoboslab.org
Phil Ashby - https://www.ashbysoft.com


-- LICENSE: The MIT License(MIT)

Copyright(c) 2021 Dominic Szablewski
Copyright(c) 2021 Phil Ashby

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#define QOI_IMPLEMENTATION
#include "qoi.h"

#include <stdint.h>
#include <string.h>

// BMP header
#pragma pack(push,1)
typedef struct {
    // bitmap file header
    uint16_t bmid;    // 'BM'
    uint32_t fsiz;    // byte count of file (inc this header)
    uint32_t resv;    // must be 0
    uint32_t bits;    // offset in file to raw bits
    // bitmap info header (DIB)
    uint32_t hsiz;    // structure size (from here)
    uint32_t cols;    // columns (width)
    uint32_t rows;    // rows (height)
    uint16_t plns;    // bit planes (must be 1)
    uint16_t bppx;    // bits per pixel (<16 => must have colour palette)
    uint32_t comp;    // compression type (we accept BI_RGB(0), BI_BITFIELDS(3))
    uint32_t isiz;    // uncompressed size in bytes (if compressed)
    uint32_t hrez;    // horizontal resolution (pix/m)
    uint32_t vrez;    // vertical resolution (pix/m)
    uint32_t npal;    // number of colours in palette (0 and bppx<16 => 2^bppx)
    uint32_t nimp;    // number of important (used) colours (0 => all)
} bitmap_load_t;
#pragma pack(pop)

void *load_bitmap(const char *file, int *pw, int *ph) {
    // BMP file loader, maps to direct colour pixel array
    bitmap_load_t bmp;
    uint8_t *tmp = NULL;
    uint32_t *pal = NULL, *rv = NULL, *img = NULL, row, bpr;
    FILE *fp = fopen(file, "rb");
    if (!fp)
        goto out;
    if (fread(&bmp, sizeof(bmp), 1, fp)!=1)
        goto out;
    // check we have a valid, usable BMP
    if (bmp.bmid!=0x4d42 ||
        bmp.hsiz<40 ||
        (bmp.comp!=0 /*&& bmp.comp!=3*/))    // TODO: support BI_BITFIELDS
        goto out;
    // seek past larger headers
    if (bmp.hsiz>40 && fseek(fp, bmp.hsiz-40, SEEK_CUR)<0)
        goto out;
    // load palette if present/required
    if (bmp.bppx<16 && 0==bmp.npal)
        bmp.npal = 1<<bmp.bppx;
    if (bmp.npal) {
        pal = calloc(bmp.npal, sizeof(uint32_t));
        if (!pal)
            goto out;
        if (fread(pal, sizeof(uint32_t), bmp.npal, fp)!=bmp.npal)
            goto out;
    }
    // allocate memory image buffer & temporary row buffer
    img = calloc(bmp.cols*bmp.rows, sizeof(uint32_t));
    bpr = (bmp.cols*bmp.bppx)/8;    // bytes per row
    tmp = malloc(bpr);
    if (!img || !tmp)
        goto out;
    // read bits, convert to pixel colours
    if (fseek(fp, bmp.bits, SEEK_SET)<0)
        goto out;
    for (row=0; row<bmp.rows; row++) {
        uint32_t col;
        if (fread(tmp, 1, bpr, fp)!=bpr)
            goto out;
        for (col=0; col<bmp.cols; col++) {
            uint32_t pix;
            if (32==bmp.bppx) {
                // BGR as lower 24 bits, same as raw pixel array
                pix = *((uint32_t *)(tmp+4*col));
            } else if (24==bmp.bppx) {
                // BGR as exact 24 bits, similar to raw pixel array
                pix = *((uint32_t *)(tmp+3*col)) & 0x00ffffff;
            } else if (16==bmp.bppx) {
                // BGR as 5:5:5 bits, high bit unused
                uint16_t val = *((uint16_t *)(tmp+2*col));
                pix = (val&0x1f)<<3 | (val&0x3e0)<<6 | (val&0x3c00)<<9;
            } else if (8==bmp.bppx) {
                // Palette mapped pixels.. 1/byte
                pix = pal[tmp[col]];
            } else if (4==bmp.bppx) {
                // Palette mapped pixels.. 2/byte
                uint8_t v = tmp[col/2];
                v = (col&0x1) ? (v>>4)&0xf : v&0xf;
                pix = pal[v];
            } else if (2==bmp.bppx) {
                // Palette mapped pixels.. 4/byte
                uint8_t v = tmp[col/4];
                uint8_t o = col%4;
                v = (3==o) ? (v>>6)&0x3 : (2==o) ? (v>>4)&0x3 : (1==o) ? (v>>2)&0x3 : v&0x3;
                pix = pal[v];
            } else if (1==bmp.bppx) {
                // Palette mapped pixels.. 8/byte
                uint8_t v = tmp[col/8];
                uint8_t o = col%8;
                v = v>>o;
                pix = pal[v];
            } else {
                // Err....
                goto out;
            }
            // DIB assumed to be bottom up, so we reverse rows as we go
            img[(bmp.rows-row-1)*bmp.cols+col] = pix;
        }
    }
    *pw = bmp.cols;
    *ph = bmp.rows;
    rv = img;
out:
    if (tmp) free(tmp);
    if (img && !rv) free(img);
    if (pal) free(pal);
    if (fp) fclose(fp);
    return rv;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s infile outfile\n", argv[0]);
        printf("Examples:\n");
        printf("  %s image.bmp image.qoi\n", argv[0]);
        exit(1);
    }

    void *pixels = NULL;
    qoi_desc desc;
    desc.channels = 4;
    desc.colorspace = 0;
    pixels = load_bitmap(argv[1], &desc.width, &desc.height);

    if (pixels == NULL) {
        printf("Couldn't load/decode %s\n", argv[1]);
        exit(1);
    }
    printf("loaded image %dx%d\n", desc.width, desc.height);
    
    int encoded = qoi_write(argv[2], pixels, &desc);

    if (!encoded) {
        printf("Couldn't write/encode %s\n", argv[2]);
        exit(1);
    }

    return 0;
}
