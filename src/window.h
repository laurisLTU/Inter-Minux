#ifndef WINDOW_H
#define WINDOW_H

typedef unsigned char u8;
typedef int s32;

typedef struct Window {
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    const char* title;
    int visible;
} Window;

enum {
    COL_BLACK = 0x00,
    COL_DARK = 0x01,
    COL_GRAY_BG = 0x07,
    COL_GRAY_BAR = 0x08,
    COL_WHITE = 0x0F
};

void gfx_set_pixel(u8* target, s32 x, s32 y, u8 c);
void gfx_fill_rect(u8* target, s32 x, s32 y, s32 w, s32 h, u8 c);
void gfx_draw_text(u8* target, s32 x, s32 y, const char* s, u8 color);
void gfx_draw_window(u8* target, const Window* w);
int gfx_window_hit_close(const Window* w, s32 mx, s32 my);

#endif