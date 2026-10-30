#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_W 640.0f
#define SCREEN_H 480.0f
#define WORLD_W 2600.0f
#define MAX_COINS 12

static GXRModeObj *rmode;
static void *xfb[2];
static int fb = 0;
static void *fifo;

typedef struct { float x, y, w, h; } Rect;
typedef struct { float x, y, vy; int grounded; int facing; } Player;

static Player mario = {70, 330, 0, 0, 1};
static float camera_x = 0;
static int coins = 0;
static int won = 0;
static int coin_taken[MAX_COINS] = {0};

static const Rect platforms[] = {
    {0, 420, 520, 60}, {570, 390, 280, 90}, {900, 350, 260, 130},
    {1220, 410, 400, 70}, {1690, 370, 300, 110}, {2070, 420, 530, 60},
    {300, 340, 100, 18}, {690, 300, 100, 18}, {1020, 270, 110, 18},
    {1450, 330, 120, 18}, {1800, 290, 120, 18}
};
#define PLATFORM_COUNT (sizeof(platforms)/sizeof(platforms[0]))

static const float coin_x[MAX_COINS] = {340, 375, 410, 735, 770, 1050, 1090, 1490, 1530, 1840, 1880, 2240};
static const float coin_y[MAX_COINS] = {300, 300, 300, 260, 260, 230, 230, 290, 290, 250, 250, 350};

static void set_color(u8 r, u8 g, u8 b, u8 a) {
    GXColor c = {r,g,b,a};
    GX_SetChanMatColor(GX_COLOR0A0, c);
}
static void quad(float x, float y, float w, float h, u8 r, u8 g, u8 b) {
    set_color(r,g,b,255);
    GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
    GX_Position3f32(x,y,0); GX_Color4u8(r,g,b,255);
    GX_Position3f32(x+w,y,0); GX_Color4u8(r,g,b,255);
    GX_Position3f32(x+w,y+h,0); GX_Color4u8(r,g,b,255);
    GX_Position3f32(x,y+h,0); GX_Color4u8(r,g,b,255);
    GX_End();
}
static void circle(float cx, float cy, float radius, u8 r, u8 g, u8 b) {
    set_color(r,g,b,255);
    const int n=20;
    GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, n+2);
    GX_Position3f32(cx,cy,0); GX_Color4u8(r,g,b,255);
    for(int i=0;i<=n;i++){
        float a=(float)i*6.2831853f/n;
        GX_Position3f32(cx+cosf(a)*radius,cy+sinf(a)*radius,0);
        GX_Color4u8(r,g,b,255);
    }
    GX_End();
}
static void pixel_text(int x, int y, const char *s, int scale) {
    // Compact 3x5 uppercase alphabet for the HUD.
    static const unsigned char font[26][5] = {
      {7,5,5,7,5},{6,5,6,5,6},{7,4,4,4,7},{6,5,5,5,6},{7,4,6,4,7},
      {7,4,6,4,4},{7,4,5,5,7},{5,5,7,5,5},{7,2,2,2,7},{3,1,1,5,7},
      {5,5,6,5,5},{4,4,4,4,7},{5,7,7,5,5},{5,7,7,7,5},{7,5,5,5,7},
      {7,5,7,4,4},{7,5,5,7,1},{7,5,7,6,5},{7,4,7,1,7},{7,2,2,2,2},
      {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},{5,5,2,2,2},{7,1,2,4,7}
    };
    for(;*s;s++,x+=4*scale){
        char c=*s;
        if(c>='a'&&c<='z') c-=32;
        if(c>='A'&&c<='Z'){
            const unsigned char *g=font[c-'A'];
            for(int row=0;row<5;row++) for(int col=0;col<3;col++)
                if(g[row]&(1<<(2-col))) quad(x+col*scale,y+row*scale,scale,scale,255,255,255);
        } else if(c>='0'&&c<='9'){
            static const unsigned char nums[10][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
            const unsigned char *g=nums[c-'0'];
            for(int row=0;row<5;row++) for(int col=0;col<3;col++)
                if(g[row]&(1<<(2-col))) quad(x+col*scale,y+row*scale,scale,scale,255,255,255);
        }
    }
}
static void draw_player(float sx, float sy) {
    // Original simple red-capped hero sprite made from GX shapes.
    quad(sx+5,sy+17,22,22,220,35,35);
    quad(sx+3,sy+12,26,8,190,25,25);
    quad(sx+7,sy+5,19,13,245,190,140);
    quad(sx+5,sy+1,23,7,210,25,25);
    quad(sx+8,sy+22,7,5,255,255,255);
    quad(sx+19,sy+22,4,5,30,30,30);
    quad(sx+6,sy+37,9,7,70,45,25);
    quad(sx+19,sy+37,9,7,70,45,25);
}
static void draw_scene(void) {
    // Sky and distant hills
    quad(0,0,640,480,115,205,250);
    circle(540,75,34,255,235,90);
    for(int i=0;i<7;i++){
        float hx=(i*150.0f-camera_x*0.22f);
        while(hx < -100) hx+=1050;
        circle(hx,400,100,105,195,105);
    }
    // Clouds
    for(int i=0;i<5;i++){
        float cx=(i*190.0f-camera_x*0.12f);
        while(cx < -80) cx+=950;
        circle(cx,105+(i%2)*28,25,255,255,255);
        circle(cx+24,100+(i%2)*28,30,255,255,255);
        circle(cx+50,108+(i%2)*28,23,255,255,255);
    }
    // World platforms
    for(unsigned i=0;i<PLATFORM_COUNT;i++){
        Rect p=platforms[i];
        float sx=p.x-camera_x;
        if(sx+p.w < -10 || sx > 650) continue;
        quad(sx,p.y,p.w,p.h,150,90,45);
        quad(sx,p.y,p.w,9,65,180,65);
        for(float bx=sx+18;bx<sx+p.w-8;bx+=38)
            quad(bx,p.y+23,2,p.h>30?18:7,115,65,35);
    }
    // Coins
    for(int i=0;i<MAX_COINS;i++) if(!coin_taken[i]){
        float x=coin_x[i]-camera_x;
        if(x>-20&&x<660){
            circle(x,coin_y[i],10,255,205,35);
            circle(x,coin_y[i],5,255,245,130);
        }
    }
    // Goal flag
    float gx=2460-camera_x;
    quad(gx,270,8,150,100,70,40);
    quad(gx+8,270,48,30,60,190,80);
    // Player
    draw_player(mario.x-camera_x,mario.y);
    // HUD panel
    quad(12,12,190,48,25,65,105);
    pixel_text(24,22,"WORLD 1-1",2);
    pixel_text(24,42,"COINS",1);
    pixel_text(76,42,"0123456789"[coins%10] ? (char[2]){'0'+coins%10,0} : "0",1);
    if(won){
        quad(115,175,410,100,20,65,100);
        pixel_text(175,205,"LEVEL CLEAR",4);
    }
}
static void setup_gx(void) {
    fifo=memalign(32,256*1024);
    memset(fifo,0,256*1024);
    GX_Init(fifo,256*1024);
    GX_SetCopyClear((GXColor){110,205,250,255},0x00ffffff);
    GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
    GX_SetScissor(0,0,rmode->fbWidth,rmode->efbHeight);
    GX_SetDispCopySrc(0,0,rmode->fbWidth,rmode->efbHeight);
    GX_SetDispCopyDst(rmode->fbWidth,rmode->xfbHeight);
    GX_SetCopyFilter(rmode->aa,rmode->sample_pattern,GX_TRUE,rmode->vfilter);
    GX_SetFieldMode(rmode->field_rendering,((rmode->viHeight==2*rmode->xfbHeight)?GX_ENABLE:GX_DISABLE));
    GX_SetPixelFmt(rmode->aa?GX_PF_RGB565_Z16:GX_PF_RGB8_Z24,GX_ZC_LINEAR);
    GX_SetCullMode(GX_CULL_NONE);
    GX_SetZMode(GX_FALSE,GX_ALWAYS,GX_FALSE);
    GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_CLEAR);
    GX_ClearVtxDesc();
    GX_SetVtxDesc(GX_VA_POS,GX_DIRECT);
    GX_SetVtxDesc(GX_VA_CLR0,GX_DIRECT);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_POS,GX_POS_XYZ,GX_F32,0);
    GX_SetVtxAttrFmt(GX_VTXFMT0,GX_VA_CLR0,GX_CLR_RGBA,GX_RGBA8,0);
    GX_SetNumChans(1);
    GX_SetChanCtrl(GX_COLOR0A0,GX_DISABLE,GX_SRC_REG,GX_SRC_REG,GX_LIGHTNULL,GX_DF_NONE,GX_AF_NONE);
    GX_SetNumTexGens(0);
    GX_SetTevOrder(GX_TEVSTAGE0,GX_TEXCOORDNULL,GX_TEXMAP_NULL,GX_COLOR0A0);
    GX_SetTevOp(GX_TEVSTAGE0,GX_PASSCLR);
    Mtx44 ortho;
    guOrtho(ortho,0,480,0,640,0,1000);
    GX_LoadProjectionMtx(ortho,GX_ORTHOGRAPHIC);
    Mtx model;
    guMtxIdentity(model);
    GX_LoadPosMtxImm(model,GX_PNMTX0);
    GX_SetDispCopyGamma(GX_GM_1_0);
}
static void update_game(void) {
    WPAD_ScanPads();
    u32 held=WPAD_ButtonsHeld(0);
    u32 down=WPAD_ButtonsDown(0);
    if(down&WPAD_BUTTON_HOME) SYS_ResetSystem(SYS_RETURNTOMENU,0,0);
    if(won) { if(down&(WPAD_BUTTON_A|WPAD_BUTTON_2)) { won=0; mario.x=70; mario.y=330; mario.vy=0; coins=0; memset(coin_taken,0,sizeof(coin_taken)); camera_x=0; } return; }
    float old_y=mario.y;
    if(held&WPAD_BUTTON_LEFT){mario.x-=3.0f;mario.facing=-1;}
    if(held&WPAD_BUTTON_RIGHT){mario.x+=3.0f;mario.facing=1;}
    if(mario.x<0)mario.x=0;
    if(mario.x>WORLD_W-32)mario.x=WORLD_W-32;
    if((down&WPAD_BUTTON_A)||(down&WPAD_BUTTON_2)){
        if(mario.grounded){mario.vy=-10.5f;mario.grounded=0;}
    }
    mario.vy+=0.48f;
    if(mario.vy>11)mario.vy=11;
    mario.y+=mario.vy;
    mario.grounded=0;
    for(unsigned i=0;i<PLATFORM_COUNT;i++){
        Rect p=platforms[i];
        if(mario.x+30>p.x&&mario.x+4<p.x+p.w&&old_y+44<=p.y+4&&mario.y+44>=p.y&&mario.vy>=0){
            mario.y=p.y-44;mario.vy=0;mario.grounded=1;
        }
    }
    if(mario.y>520){mario.x=70;mario.y=330;mario.vy=0;camera_x=0;}
    for(int i=0;i<MAX_COINS;i++) if(!coin_taken[i]){
        float dx=(mario.x+17)-coin_x[i],dy=(mario.y+20)-coin_y[i];
        if(dx*dx+dy*dy<28*28){coin_taken[i]=1;coins++;}
    }
    if(mario.x>2440)won=1;
    camera_x=mario.x-230;
    if(camera_x<0)camera_x=0;
    if(camera_x>WORLD_W-SCREEN_W)camera_x=WORLD_W-SCREEN_W;
}
int main(void) {
    VIDEO_Init();
    WPAD_Init();
    rmode=VIDEO_GetPreferredMode(NULL);
    xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[fb]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
    fb^=1;
    setup_gx();
    while(1){
        update_game();
        GX_SetViewport(0,0,rmode->fbWidth,rmode->efbHeight,0,1);
        GX_InvVtxCache();
        GX_InvalidateTexAll();
        draw_scene();
        GX_DrawDone();
        GX_CopyDisp(xfb[fb],GX_TRUE);
        VIDEO_SetNextFramebuffer(xfb[fb]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb^=1;
    }
    return 0;
}
