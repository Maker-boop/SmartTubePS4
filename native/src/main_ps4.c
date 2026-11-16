/*
 * SmartTube PS4 v2.9 — native OpenOrbis build.
 *
 * Draws the milestone screen straight into a libSceVideoOut framebuffer,
 * polls the DualShock 4 through libScePad, and brings up libSceAudioOut and
 * libSceNet so later milestones have real audio/network paths to build on.
 * No SDL2 anywhere.
 */

/* sys/types.h first: the OpenOrbis kernel types use u_short/u_int. */
#include <sys/types.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <orbis/AudioOut.h>
#include <orbis/Net.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>

#define SCREEN_W 1920
#define SCREEN_H 1080
#define BYTES_PER_PIXEL 4
#define FRAME_BUFFER_COUNT 3
#define FRAME_BUFFER_ALIGN 0x200000
#define FRAME_INTERVAL_US 16000
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_GRAIN 512

/* Literal values rather than the ORBIS_VIDEO_OUT_* names: the OpenOrbis
 * release the CI container ships (v0.5.1) does not define them. */
#define VIDEO_PIXEL_FORMAT 0x80000000u /* A8R8G8B8; pixel value is 0x80RRGGBB */
#define VIDEO_TILING_LINEAR 1
#define VIDEO_ASPECT_RATIO_16_9 0
#define OPAQUE 0x80000000u

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return OPAQUE | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* ---------------------------------------------------------------- user --- */

static int32_t g_user_id = -1;

static int init_user(void) {
    OrbisUserServiceInitializeParams params;
    int32_t rc;

    memset(&params, 0, sizeof(params));
    params.priority = 0x2BC;
    rc = sceUserServiceInitialize(&params);
    if (rc != 0 && rc != (int32_t)0x80960002u)
        fprintf(stderr, "sceUserServiceInitialize: 0x%08x\n", (uint32_t)rc);

    rc = sceUserServiceGetInitialUser(&g_user_id);
    if (rc != 0 || g_user_id < 0) {
        fprintf(stderr, "No signed-in user available.\n");
        g_user_id = -1;
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------- video --- */

static int32_t g_video_handle = -1;
static void *g_video_mem = NULL;
static off_t g_video_mem_offset = 0;
static size_t g_video_mem_size = 0;
static uint32_t *g_frames[FRAME_BUFFER_COUNT];
static int g_frame_index = 0;
static int64_t g_flip_arg = 0;

static void shutdown_video(void) {
    if (g_video_handle >= 0) {
        sceVideoOutUnregisterBuffers(g_video_handle, 0);
        sceVideoOutClose(g_video_handle);
        g_video_handle = -1;
    }
    if (g_video_mem != NULL) {
        sceKernelMunmap(g_video_mem, g_video_mem_size);
        g_video_mem = NULL;
    }
    if (g_video_mem_size != 0) {
        sceKernelReleaseDirectMemory(g_video_mem_offset, g_video_mem_size);
        g_video_mem_offset = 0;
        g_video_mem_size = 0;
    }
}

static int init_video(void) {
    const size_t frame_size = (size_t)SCREEN_W * SCREEN_H * BYTES_PER_PIXEL;
    OrbisVideoOutBufferAttribute attr;
    int32_t rc;
    size_t i;

    g_video_handle = sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN, ORBIS_VIDEO_OUT_BUS_MAIN, 0, NULL);
    if (g_video_handle < 0) {
        fprintf(stderr, "sceVideoOutOpen: 0x%08x\n", (uint32_t)g_video_handle);
        g_video_handle = -1;
        return -1;
    }

    g_video_mem_size = ((frame_size * FRAME_BUFFER_COUNT) + FRAME_BUFFER_ALIGN - 1) /
                       FRAME_BUFFER_ALIGN * FRAME_BUFFER_ALIGN;

    rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), g_video_mem_size,
                                       FRAME_BUFFER_ALIGN, 3, &g_video_mem_offset);
    if (rc < 0) {
        fprintf(stderr, "sceKernelAllocateDirectMemory: 0x%08x\n", (uint32_t)rc);
        shutdown_video();
        return -1;
    }

    rc = sceKernelMapDirectMemory(&g_video_mem, g_video_mem_size, 0x33, 0,
                                  g_video_mem_offset, FRAME_BUFFER_ALIGN);
    if (rc < 0) {
        fprintf(stderr, "sceKernelMapDirectMemory: 0x%08x\n", (uint32_t)rc);
        g_video_mem = NULL;
        shutdown_video();
        return -1;
    }

    memset(g_video_mem, 0, g_video_mem_size);
    for (i = 0; i < FRAME_BUFFER_COUNT; ++i)
        g_frames[i] = (uint32_t *)((uint8_t *)g_video_mem + frame_size * i);

    sceVideoOutSetBufferAttribute(&attr, VIDEO_PIXEL_FORMAT, VIDEO_TILING_LINEAR,
                                  VIDEO_ASPECT_RATIO_16_9, SCREEN_W, SCREEN_H, SCREEN_W);
    rc = sceVideoOutRegisterBuffers(g_video_handle, 0, (void *const *)g_frames,
                                    FRAME_BUFFER_COUNT, &attr);
    if (rc < 0) {
        fprintf(stderr, "sceVideoOutRegisterBuffers: 0x%08x\n", (uint32_t)rc);
        shutdown_video();
        return -1;
    }

    return 0;
}

static void present_frame(void) {
    if (g_video_handle < 0)
        return;
    sceVideoOutSubmitFlip(g_video_handle, g_frame_index, ORBIS_VIDEO_OUT_FLIP_VSYNC, g_flip_arg++);
    g_frame_index = (g_frame_index + 1) % FRAME_BUFFER_COUNT;
}

/* ---------------------------------------------------------- controller --- */

static int g_pad_handle = -1;
static int g_pad_ready = 0;

static int init_controller(void) {
    int32_t rc;

    if (g_user_id < 0) {
        fprintf(stderr, "No signed-in user available for Pad.\n");
        return -1;
    }

    rc = scePadInit();
    if (rc != 0) {
        fprintf(stderr, "scePadInit: 0x%08x\n", (uint32_t)rc);
        return -1;
    }

    g_pad_handle = scePadOpen(g_user_id, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);
    if (g_pad_handle < 0) {
        fprintf(stderr, "scePadOpen: 0x%08x\n", (uint32_t)g_pad_handle);
        g_pad_handle = -1;
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

static void shutdown_controller(void) {
    if (g_pad_handle >= 0) {
        scePadClose(g_pad_handle);
        g_pad_handle = -1;
    }
    g_pad_ready = 0;
}

/* --------------------------------------------------------------- audio --- */

static int32_t g_audio_port = -1;

static int init_audio(void) {
    int32_t rc;

    rc = sceAudioOutInit();
    if (rc < 0)
        fprintf(stderr, "sceAudioOutInit: 0x%08x\n", (uint32_t)rc);

    g_audio_port = sceAudioOutOpen(
        (g_user_id >= 0) ? g_user_id : ORBIS_USER_SERVICE_USER_ID_SYSTEM,
        ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, AUDIO_GRAIN, AUDIO_SAMPLE_RATE,
        ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
    if (g_audio_port < 0) {
        fprintf(stderr, "sceAudioOutOpen: 0x%08x\n", (uint32_t)g_audio_port);
        g_audio_port = -1;
        return -1;
    }

    return 0;
}

static void shutdown_audio(void) {
    if (g_audio_port >= 0) {
        sceAudioOutClose(g_audio_port);
        g_audio_port = -1;
    }
}

/* ----------------------------------------------------------------- net --- */

static int g_net_ready = 0;

static int init_net(void) {
    /* The v0.5.1 headers the CI container ships declare sceNetInit() as void
     * and expose no net sysmodule id, so the return value is not checked and
     * the internal sysmodule load is left to libSceNet itself. */
    sceNetInit();
    g_net_ready = 1;
    return 0;
}

static void shutdown_net(void) {
    if (g_net_ready) {
        sceNetTerm();
        g_net_ready = 0;
    }
}

/* ------------------------------------------------------------- drawing --- */

static const uint8_t font[36][5] = {
{0x3e,0x51,0x49,0x45,0x3e},{0x00,0x42,0x7f,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1e},
{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},{0x7f,0x49,0x49,0x49,0x41},{0x7f,0x09,0x09,0x09,0x01},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,0x08,0x08,0x08,0x7f},{0x00,0x41,0x7f,0x41,0x00},{0x20,0x40,0x41,0x3f,0x01},
{0x7f,0x08,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,0x02,0x0c,0x02,0x7f},{0x7f,0x04,0x08,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,0x09,0x09,0x09,0x06},{0x3e,0x41,0x51,0x21,0x5e},{0x7f,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7f,0x01,0x01},
{0x3f,0x40,0x40,0x40,0x3f},{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
};

static void put_pixel(uint32_t *fb, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= SCREEN_W || y >= SCREEN_H)
        return;
    fb[y * SCREEN_W + x] = color;
}

static void fill(uint32_t *fb, int x, int y, int w, int h, uint32_t color) {
    int px, py;

    for (py = y; py < y + h; ++py)
        for (px = x; px < x + w; ++px)
            put_pixel(fb, px, py, color);
}

static void draw_text(uint32_t *fb, int x, int y, const char *s, int scale, uint32_t color) {
    for (; *s; ++s, x += 6 * scale) {
        int idx = -1;
        char c = *s;
        int col, row;

        if (c >= '0' && c <= '9')
            idx = c - '0';
        else if (c >= 'A' && c <= 'Z')
            idx = 10 + c - 'A';
        if (idx < 0)
            continue;

        for (col = 0; col < 5; ++col)
            for (row = 0; row < 7; ++row)
                if (font[idx][col] & (1u << row))
                    fill(fb, x + col * scale, y + row * scale, scale, scale, color);
    }
}

static void render_screen(uint32_t buttons) {
    uint32_t *fb = g_frames[g_frame_index];

    if (fb == NULL)
        return;

    fill(fb, 0, 0, SCREEN_W, SCREEN_H, rgb(9, 13, 24));
    fill(fb, 70, 65, 1780, 8, rgb(38, 126, 255));
    draw_text(fb, 100, 115, "SMARTTUBE PS4", 8, rgb(245, 248, 255));
    draw_text(fb, 103, 205, "V2 9 DEV BUILD", 4, rgb(100, 174, 255));
    fill(fb, 100, 330, 1720, 500, rgb(18, 27, 45));
    fill(fb, 135, 365, 1650, 5, rgb(38, 126, 255));
    draw_text(fb, 180, 430, "DUALSHOCK 4 INPUT", 5, rgb(245, 248, 255));
    fill(fb, 180, 535, 700, 125,
         rgb(g_pad_ready ? 18 : 65, g_pad_ready ? 105 : 40, 70));
    draw_text(fb, 215, 575, g_pad_ready ? "CONTROLLER READY" : "CONTROLLER WAIT", 3,
              rgb(255, 255, 255));
    fill(fb, 930, 535, 770, 125, rgb(24, 39, 62));
    draw_text(fb, 970, 575, (buttons & ORBIS_PAD_BUTTON_CROSS) ? "CROSS PRESSED" : "PRESS CROSS",
              3, rgb(255, 255, 255));
    draw_text(fb, 180, 730, "OPTIONS EXIT", 4, rgb(145, 180, 255));
    draw_text(fb, 180, 900, "V2 9 MILESTONE BASIC SCREEN AND DS4 POLLING", 2, rgb(120, 139, 165));
}

/* ---------------------------------------------------------------- main --- */

int main(int argc, char **argv) {
    int running = 1;

    (void)argc;
    (void)argv;

    setvbuf(stdout, NULL, _IONBF, 0);

    init_user();
    if (init_video() != 0) {
        fprintf(stderr, "Video output unavailable, cannot draw the milestone screen.\n");
        shutdown_video();
        return 1;
    }
    init_controller();
    init_audio();
    init_net();

    while (running) {
        uint32_t buttons = read_buttons();

        if (buttons & ORBIS_PAD_BUTTON_OPTIONS)
            running = 0;

        render_screen(buttons);
        present_frame();
        sceKernelUsleep(FRAME_INTERVAL_US);
    }

    shutdown_net();
    shutdown_audio();
    shutdown_controller();
    shutdown_video();
    return 0;
}
