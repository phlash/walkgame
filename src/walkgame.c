// a simple game based on walking around a vector-drawn map..

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#include <dos.h>
#include <string.h>
#include <math.h>
#include <drawlib.h>

// keyboard input, requires an interrupt handler..
#define WGK_ESC				0x01
#define WGK_ENTER			0x1C
#define WGK_ACTION			0x1F //S
#define WGK_JUMP			0x20 //D
#define WGK_UP				0x48
#define WGK_DOWN			0x50
#define WGK_LEFT			0x4B
#define WGK_RIGHT			0x4D

static FILE *wg_log;

static unsigned char wg_keys[128];
static void interrupt (*wg_oldk)(void);
static void interrupt wg_newk(void) {
	unsigned char keyhit;
	asm{
    pushf
	cli
    in    al, 060h      
    mov   keyhit, al
    in    al, 061h       
    mov   bl, al
    or    al, 080h
    out   061h, al     
    mov   al, bl
    out   061h, al      
    mov   al, 020h       
    out   020h, al
    popf
    }
	if (keyhit & 0x80){ keyhit &= 0x7F; wg_keys[keyhit] = 0;} //KEY_RELEASED;
    else wg_keys[keyhit] = 1; //KEY_PRESSED;
}
static void wg_installkeys(void) {
    memset(wg_keys, 0, sizeof(wg_keys));
    wg_oldk = getvect(9);
    setvect(9, wg_newk);
}
static void wg_removekeys(void) {
    if (wg_oldk)
        setvect(9, wg_oldk);
}

// text output
int wg_textout(int x, int y, const char *msg) {
    static void *font;
    static int flen;
    static int offsets[64];
    int n;
    int o = 0;

    if (!font) {
        font = load_bitmap("GFX/doomfont.bmp", &flen, 0);
        if (!font)
            return -1;

        for(n=0;n<dimagewidth(font);n++){
            if (dimagegetpixel(font,flen,n,0) == 122){
                offsets[o] = n;
                if (wg_log) fprintf(wg_log,"offset added %d %d \n",o, n);
                o++;
                if(o >= 64){
                    break;
                }
            }
        }
    }

    for (n=0; msg[n]; n++) {
        if (msg[n]>=32 && msg[n]<123) {
            int fi = (int)msg[n] - 33;
            if (fi<0){
                //it's a space
                x += 8;
            }else{
                int w = offsets[fi+1]-offsets[fi];
                dimageput(x,y, offsets[fi], 1, w, dimageheight(font)-1, font, flen, 247);
                x += w; 
            }
            
        } else {
            x += 8;
        }
    }
    return n;
}

// collision detector
int collide(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    // simple rectangular overlap check
    return (x1<x2+w2 && x2<x1+w1 && y1<y2+h2 && y2<y1+h1) ? 1 : 0;
}

// game loop..
#define MAX_OBS 15          // maximum no of obstacles
#define OBS_TCK 15          // ticks (18.2Hz) between obstacle spawns
static int s_nobs;          // spawned obstacle count
static int s_obs[MAX_OBS][3];// co-ordinates and y movement of obstacles
#define WIN_GOALS   10      // number of times to hit the goal to win!!

#define NTRIG 72            // pre-calulated trig values
static int s_sin[NTRIG], s_cos[NTRIG];

int main(int argc, char **argv) {
    int done=0, frms=0, sfps=0, ngoal=0, lg=0, i, x, y, gx, gy;
    clock_t beg, now, lst;
    void *play, *obj, *goal, *boom;
    int plen, olen, glen, blen;
    char *err=NULL;

    //movement variables
    int velX=0, velY=0;
    int drag = 2;
    int accel = 7;

    dmode_t dm = DMODE_HIGHEST;

    for (i=1; i<argc; i++) {
        if (strncmp(argv[i],"-f",2)==0)
            sfps=1;
        //else if (strncmp(argv[i],"-w",2)==0)
        //    wnam=argv[++i];
        else if (strncmp(argv[i],"-l",2)==0)
            wg_log=fopen(argv[++i], "wt");
        else if (strncmp(argv[i],"-3",2)==0)
            dm = DMODE_320x200;
        else if (strncmp(argv[i],"-6",2)==0)
            dm = DMODE_640x480;
        else if (strncmp(argv[i],"-8",2)==0)
            dm = DMODE_800x600;
        else
            return printf("usage: %s [-f (show fps)] [-w <wad file>] [-l <wg_log file>]\n", argv[0]);
    }
    if (wg_log) fprintf(wg_log, "starting\n");
    if (videomode(dm)) {
        fputs("unable to enter graphics mode :=(", stderr);
        return 1;
    }
    // load player avatar and set palette..
    err="player";
    play=load_bitmap("GFX/penguin.bmp", &plen, 1);
    if (!play)
        goto oops;
    // load obstacle image
    err="object";
    obj=load_bitmap("GFX/apple.bmp", &olen, 0);
    if (!obj)
        goto oops;
    // load goal image
    err="goal";
    goal=load_bitmap("GFX/loading.bmp", &glen, 0);
    if (!goal)
        goto oops;
    // load explosion image
    err="boom";
    boom=load_bitmap("GFX/boom.bmp", &blen, 0);
    if (!boom)
        goto oops;
    err=NULL;
    for(i=0;; i+=10) {
        wg_textout(i, i, "TEST ME!");
        drefresh(0);
        if (0x1b==getch())
            break;
    }
    goto oops;
    // grab keyboard
    wg_installkeys();
    // instructions!
    while (!wg_keys[WGK_ENTER] && !wg_keys[WGK_ESC]) {
        wg_textout(10, 10, "EVIL APPLES WILL EAT YOU");
        wg_textout(10, 30, "REACH THE ORANGE PORTAL 10 TIMES");
        wg_textout(10, 50, "TO ESCAPE!");
        wg_textout(40, NUM_ROW-20, "PRESS ENTER TO PLAY...");
        waitV();
        drefresh(0);
        printf("refresh\n");
    }
    if (wg_keys[WGK_ESC])
        goto oops;
    // generate sin/cos tables, scaled to NUM_ROW/3
    for (i=0; i<NTRIG; i++) {
        double a = (double)i*2.0*M_PI/(double)NTRIG;
        s_sin[i] = (int)(sin(a)*(double)NUM_ROW/3);
        s_cos[i] = (int)(cos(a)*(double)NUM_ROW/3);
    }
    // time stuff..
    lst=now=beg=clock();
    x = (ROW_LEN-dimagewidth(play))/2;
    y = (NUM_ROW-dimageheight(play))/2;
    gx = ROW_LEN/2+s_cos[0];
    gy = NUM_ROW/2-s_sin[0];
    while (!done) {
        static char gls[10];
        // animate goal, circle round the screen.. !!CHANGED SO GOAL MOVES TO RANDOM POSITION INSTEAD!!
        //gx = (ROW_LEN-32)/2+s_cos[now%NTRIG];
        //gy = (NUM_ROW-32)/2+s_sin[now%NTRIG];
        dimageput(gx, gy, (frms&0x3)*32, (frms&0x4)?32:0, 32, 32, goal, glen, 240);
        // spawn new obstacle?
        if (s_nobs<MAX_OBS && lst+OBS_TCK<now) {
            s_obs[s_nobs][0]=0;
            s_obs[s_nobs][1]=random(NUM_ROW-dimageheight(obj));
            s_obs[s_nobs][2]=random(5)-2;
            if (wg_log) fprintf(wg_log, "spawn: %d@%d/%d/%d\n", s_nobs, s_obs[s_nobs][0], s_obs[s_nobs][1], s_obs[s_nobs][2]);
            s_nobs+=1;
            lst=now;
        }
        // move then remove or draw each obstacle
        for (i=0; i<s_nobs; i++) {
            int oy;
            // steady left->right..
            s_obs[i][0]+=3;
            // randomized up/down (with bouncing)
            oy=s_obs[i][1]+s_obs[i][2];
            if (oy<0) {
                oy=0;
                s_obs[i][2]=-s_obs[i][2];
            } else if (oy>NUM_ROW-dimageheight(obj)) {
                oy=NUM_ROW-dimageheight(obj);
                s_obs[i][2]=-s_obs[i][2];
            }
            s_obs[i][1]=oy;
            if (s_obs[i][0]>ROW_LEN-dimagewidth(obj)) {
                memmove(&s_obs[i][0], &s_obs[i+1][0], (s_nobs-i-1)*3*sizeof(int));
                s_nobs-=1;
                i-=1;
                if (wg_log) fprintf(wg_log, "rem: %d\n", i+1);
            } else {
                dimageput(s_obs[i][0], oy, 0, 0, 0, 0, obj, olen, -1);
                //if (wg_log) fprintf(wg_log, "draw: %d@%d/%d\n", i, obs[i][0], obs[i][1]);
            }
        }
        // update player position..
        if (wg_keys[WGK_LEFT])
            velX-=accel;
        if (wg_keys[WGK_RIGHT])
            velX+=accel;
        if (wg_keys[WGK_UP])
            velY-=accel;
        if (wg_keys[WGK_DOWN])
            velY+=accel;

        velX = velX/drag;
        velY = velY/drag;

        x += velX;
        y += velY;

        x = (x<0) ? 0 : (x>ROW_LEN-dimagewidth(play)) ? ROW_LEN-dimagewidth(play) : x;
        y = (y<0) ? 0 : (y>NUM_ROW-dimageheight(play)) ? NUM_ROW-dimageheight(play) : y;
        //if (wg_log) fprintf(wg_log, "play@%d/%d\n", x, y);
        // collision?
        for (i=0; i<s_nobs; i++) {
            if (collide(x, y, dimagewidth(play), dimageheight(play), s_obs[i][0], s_obs[i][1], dimagewidth(obj), dimageheight(obj))) {
                wg_textout(280, 10, "BOOM!");
                dimageput(x>8?x-8:0, y>8?y-8:0, 0, 0, 0, 0, boom, blen, 0);
                if (ngoal<WIN_GOALS) ngoal=0;
                if (wg_log) fprintf(wg_log, "hit: %d@%d/%d\n", i, s_obs[i][0], s_obs[i][1]);
                break;
            }
        }
        // no collision..
        if (i>=s_nobs) {
            dimageput(x, y, 0, 0, 0, 0, play, plen, -1);
            // hit the goal?
            if (collide(x, y, dimagewidth(play), dimageheight(play), gx, gy, 32, 32)) {
                if (!lg) {
                    lg=1;
                    if (ngoal<WIN_GOALS) ngoal+=1;
                    if (wg_log) fprintf(wg_log, "goal@%d/%d\n", x, y);
                    gx = rand()% (ROW_LEN-32-64) + 64;
                    gy = rand()% (NUM_ROW-32);
                }
            } else {
                lg=0;
            }
        }
        sprintf(gls, "GOALS:%d", ngoal);
        wg_textout(10, 10, gls);
        if (ngoal>=WIN_GOALS) {
            wg_textout(x, y, "YOU WIN! HIT ESC");
        }
        // done?
        if (wg_keys[WGK_ESC])
            done=1;
        waitV();
        drefresh(194);
        now=clock();
        if (sfps && now>beg) {
            static char fps[16];
            sprintf(fps, "FPS:%.2f", ((float)frms*CLK_TCK)/(float)(now-beg));
            if (wg_textout(2, NUM_ROW-10, fps)<0)
                if (wg_log) fprintf(wg_log, "textout fail!\n");
        }
        frms+=1;
    }

    while (ngoal >= WIN_GOALS && !wg_keys[WGK_ENTER]) {
        wg_textout(10, 10, "WOW");
        wg_textout(10, 30, "YOU DID IT");
        wg_textout(10, 50, "HERES A NUMBER:12346");
        wg_textout(40, NUM_ROW-20, "PRESS ENTER TO QUIT...");
        waitV();
        drefresh(0);
    }
oops:
    // ungrab keyboard
    wg_removekeys();
    // back to prev video mode
    restoremode();
    if (err)
        puts(err);
    if (wg_log)
        fclose(wg_log);
    return 0;
}