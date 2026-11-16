#include <SDL2/SDL.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SCREEN_W 1920
#define SCREEN_H 1080
static int g_pad_handle = -1;
static int g_pad_ready = 0;
static int init_controller(void) {
    int32_t user_id = -1;
    int rc;
    OrbisUserServiceInitializeParams params;
    memset(&params, 0, sizeof(params));
    params.priority = 0x2BC;
    rc = sceUserServiceInitialize(&params);
    if (rc != 0 && rc != 0x80960002)
        fprintf(stderr, "sceUserServiceInitialize: 0x%08x\n", rc);
    rc = sceUserServiceGetInitialUser(&user_id);
    if (rc != 0 || user_id < 0) {
        fprintf(stderr, "No signed-in user available for Pad.\n");
        return -1;
    }
    rc = scePadInit();
    if (rc != 0) {
        fprintf(stderr, "scePadInit: 0x%08x\n", rc);
        return -1;
    }
    g_pad_handle = scePadOpen(user_id, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (g_pad_handle < 0) {
        fprintf(stderr, "scePadOpen: 0x%08x\n", g_pad_handle);
        return -1;
    }
    g_pad_ready = 1;
    return 0;
}
static uint32_t read_buttons(void) {
    OrbisPadData data;
    memset(&data, 0, sizeof(data));
    if (!g_pad_ready || g_pad_handle < 0 || scePadReadState(g_pad_handle, &data) != 0)
        return 0;
    return data.buttons;
}
static const uint8_t font[36][5] = {
{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};
static void draw_text(SDL_Renderer *r,int x,int y,const char *s,int scale,Uint8 cr,Uint8 cg,Uint8 cb) {
    SDL_SetRenderDrawColor(r,cr,cg,cb,255);
    for (;*s;++s,x+=6*scale) {
        int idx=-1; char c=*s;
        if(c>='0'&&c<='9') idx=c-'0'; else if(c>='A'&&c<='Z') idx=10+c-'A';
        if(idx<0) continue;
        for(int col=0;col<5;++col) for(int row=0;row<7;++row)
            if(font[idx][col]&(1u<<row)){SDL_Rect p={x+col*scale,y+row*scale,scale,scale};SDL_RenderFillRect(r,&p);}
    }
}
static void fill(SDL_Renderer *r,int x,int y,int w,int h,Uint8 cr,Uint8 cg,Uint8 cb) {
    SDL_Rect q={x,y,w,h}; SDL_SetRenderDrawColor(r,cr,cg,cb,255); SDL_RenderFillRect(r,&q);
}
static void render_screen(SDL_Renderer *r,uint32_t buttons) {
    SDL_SetRenderDrawColor(r,9,13,24,255); SDL_RenderClear(r);
    fill(r,70,65,1780,8,38,126,255);
    draw_text(r,100,115,"SMARTTUBE PS4",8,245,248,255);
    draw_text(r,103,205,"V2 9 DEV BUILD",4,100,174,255);
    fill(r,100,330,1720,500,18,27,45);
    fill(r,135,365,1650,5,38,126,255);
    draw_text(r,180,430,"DUALSHOCK 4 INPUT",5,245,248,255);
    fill(r,180,535,700,125,g_pad_ready?18:65,g_pad_ready?105:40,70);
    draw_text(r,215,575,g_pad_ready?"CONTROLLER READY":"CONTROLLER WAIT",3,255,255,255);
    fill(r,930,535,770,125,24,39,62);
    draw_text(r,970,575,(buttons&ORBIS_PAD_BUTTON_CROSS)?"CROSS PRESSED":"PRESS CROSS",3,255,255,255);
    draw_text(r,180,730,"OPTIONS EXIT",4,145,180,255);
    draw_text(r,180,900,"V2 9 MILESTONE BASIC SCREEN AND DS4 POLLING",2,120,139,165);
    SDL_RenderPresent(r);
}
static void shutdown_controller(void) {
    if(g_pad_handle>=0){scePadClose(g_pad_handle);g_pad_handle=-1;}
    g_pad_ready=0;
}
int main(int argc,char **argv) {
    SDL_Window *window=NULL; SDL_Renderer *renderer=NULL; int running=1;
    (void)argc; (void)argv;
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)!=0){fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());return 1;}
    window=SDL_CreateWindow("SmartTube PS4 v2.9",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,SCREEN_W,SCREEN_H,SDL_WINDOW_SHOWN);
    if(!window){fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());SDL_Quit();return 2;}
    renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
    if(!renderer) renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
    if(!renderer){fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());SDL_DestroyWindow(window);SDL_Quit();return 3;}
    (void)init_controller();
    while(running){SDL_Event event; uint32_t buttons;
        while(SDL_PollEvent(&event)) if(event.type==SDL_QUIT) running=0;
        buttons=read_buttons();
        if(buttons&ORBIS_PAD_BUTTON_OPTIONS) running=0;
        render_screen(renderer,buttons); SDL_Delay(16);
    }
    shutdown_controller(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); return 0;
}
