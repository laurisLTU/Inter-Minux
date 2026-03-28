#include "window.h"

#define WIDTH 320
#define HEIGHT 200

static void draw_glyph_5x7_on(u8* target, s32 x, s32 y, const u8 rows[7], u8 color) {
    s32 ry;
    s32 rx;
    for (ry = 0; ry < 7; ry++) {
        for (rx = 0; rx < 5; rx++) {
            if ((rows[ry] >> (4 - rx)) & 1) {
                gfx_set_pixel(target, x + rx, y + ry, color);
            }
        }
    }
}

static int glyph_for_char(char c, u8 out[7]) {
    u8 i;
    for (i = 0; i < 7; i++) out[i] = 0;

    if (c >= 'a' && c <= 'z') {
        c = (char)(c - ('a' - 'A'));
    }

    switch (c) {
        case 'A': out[0]=0x0E; out[1]=0x11; out[2]=0x11; out[3]=0x1F; out[4]=0x11; out[5]=0x11; out[6]=0x11; return 1;
        case 'B': out[0]=0x1E; out[1]=0x11; out[2]=0x11; out[3]=0x1E; out[4]=0x11; out[5]=0x11; out[6]=0x1E; return 1;
        case 'C': out[0]=0x0E; out[1]=0x11; out[2]=0x10; out[3]=0x10; out[4]=0x10; out[5]=0x11; out[6]=0x0E; return 1;
        case 'D': out[0]=0x1C; out[1]=0x12; out[2]=0x11; out[3]=0x11; out[4]=0x11; out[5]=0x12; out[6]=0x1C; return 1;
        case 'E': out[0]=0x1F; out[1]=0x10; out[2]=0x10; out[3]=0x1E; out[4]=0x10; out[5]=0x10; out[6]=0x1F; return 1;
        case 'F': out[0]=0x1F; out[1]=0x10; out[2]=0x10; out[3]=0x1E; out[4]=0x10; out[5]=0x10; out[6]=0x10; return 1;
        case 'G': out[0]=0x0E; out[1]=0x11; out[2]=0x10; out[3]=0x17; out[4]=0x11; out[5]=0x11; out[6]=0x0E; return 1;
        case 'H': out[0]=0x11; out[1]=0x11; out[2]=0x11; out[3]=0x1F; out[4]=0x11; out[5]=0x11; out[6]=0x11; return 1;
        case 'I': out[0]=0x1F; out[1]=0x04; out[2]=0x04; out[3]=0x04; out[4]=0x04; out[5]=0x04; out[6]=0x1F; return 1;
        case 'K': out[0]=0x11; out[1]=0x12; out[2]=0x14; out[3]=0x18; out[4]=0x14; out[5]=0x12; out[6]=0x11; return 1;
        case 'L': out[0]=0x10; out[1]=0x10; out[2]=0x10; out[3]=0x10; out[4]=0x10; out[5]=0x10; out[6]=0x1F; return 1;
        case 'M': out[0]=0x11; out[1]=0x1B; out[2]=0x15; out[3]=0x11; out[4]=0x11; out[5]=0x11; out[6]=0x11; return 1;
        case 'N': out[0]=0x11; out[1]=0x19; out[2]=0x15; out[3]=0x13; out[4]=0x11; out[5]=0x11; out[6]=0x11; return 1;
        case 'O': out[0]=0x0E; out[1]=0x11; out[2]=0x11; out[3]=0x11; out[4]=0x11; out[5]=0x11; out[6]=0x0E; return 1;
        case 'P': out[0]=0x1E; out[1]=0x11; out[2]=0x11; out[3]=0x1E; out[4]=0x10; out[5]=0x10; out[6]=0x10; return 1;
        case 'R': out[0]=0x1E; out[1]=0x11; out[2]=0x11; out[3]=0x1E; out[4]=0x14; out[5]=0x12; out[6]=0x11; return 1;
        case 'S': out[0]=0x0F; out[1]=0x10; out[2]=0x10; out[3]=0x0E; out[4]=0x01; out[5]=0x01; out[6]=0x1E; return 1;
        case 'T': out[0]=0x1F; out[1]=0x04; out[2]=0x04; out[3]=0x04; out[4]=0x04; out[5]=0x04; out[6]=0x04; return 1;
        case 'U': out[0]=0x11; out[1]=0x11; out[2]=0x11; out[3]=0x11; out[4]=0x11; out[5]=0x11; out[6]=0x0E; return 1;
        case 'V': out[0]=0x11; out[1]=0x11; out[2]=0x11; out[3]=0x11; out[4]=0x11; out[5]=0x0A; out[6]=0x04; return 1;
        case 'W': out[0]=0x11; out[1]=0x11; out[2]=0x11; out[3]=0x15; out[4]=0x15; out[5]=0x1B; out[6]=0x11; return 1;
        case 'X': out[0]=0x11; out[1]=0x11; out[2]=0x0A; out[3]=0x04; out[4]=0x0A; out[5]=0x11; out[6]=0x11; return 1;
        case 'Y': out[0]=0x11; out[1]=0x11; out[2]=0x0A; out[3]=0x04; out[4]=0x04; out[5]=0x04; out[6]=0x04; return 1;
        case '-': out[0]=0x00; out[1]=0x00; out[2]=0x00; out[3]=0x1F; out[4]=0x00; out[5]=0x00; out[6]=0x00; return 1;
        case ':': out[0]=0x00; out[1]=0x04; out[2]=0x00; out[3]=0x00; out[4]=0x00; out[5]=0x04; out[6]=0x00; return 1;
        case '=': out[0]=0x00; out[1]=0x1F; out[2]=0x00; out[3]=0x1F; out[4]=0x00; out[5]=0x00; out[6]=0x00; return 1;
        case '+': out[0]=0x00; out[1]=0x04; out[2]=0x04; out[3]=0x1F; out[4]=0x04; out[5]=0x04; out[6]=0x00; return 1;
        case '.': out[0]=0x00; out[1]=0x00; out[2]=0x00; out[3]=0x00; out[4]=0x00; out[5]=0x06; out[6]=0x06; return 1;
        case ' ': return 1;
        case '0': out[0]=0x0E; out[1]=0x11; out[2]=0x13; out[3]=0x15; out[4]=0x19; out[5]=0x11; out[6]=0x0E; return 1;
        case '1': out[0]=0x04; out[1]=0x0C; out[2]=0x04; out[3]=0x04; out[4]=0x04; out[5]=0x04; out[6]=0x0E; return 1;
        case '2': out[0]=0x0E; out[1]=0x11; out[2]=0x01; out[3]=0x02; out[4]=0x04; out[5]=0x08; out[6]=0x1F; return 1;
        case '3': out[0]=0x1E; out[1]=0x01; out[2]=0x01; out[3]=0x0E; out[4]=0x01; out[5]=0x01; out[6]=0x1E; return 1;
        case '4': out[0]=0x02; out[1]=0x06; out[2]=0x0A; out[3]=0x12; out[4]=0x1F; out[5]=0x02; out[6]=0x02; return 1;
        case '5': out[0]=0x1F; out[1]=0x10; out[2]=0x10; out[3]=0x1E; out[4]=0x01; out[5]=0x01; out[6]=0x1E; return 1;
        case '6': out[0]=0x0E; out[1]=0x10; out[2]=0x10; out[3]=0x1E; out[4]=0x11; out[5]=0x11; out[6]=0x0E; return 1;
        case '7': out[0]=0x1F; out[1]=0x01; out[2]=0x02; out[3]=0x04; out[4]=0x08; out[5]=0x08; out[6]=0x08; return 1;
        case '8': out[0]=0x0E; out[1]=0x11; out[2]=0x11; out[3]=0x0E; out[4]=0x11; out[5]=0x11; out[6]=0x0E; return 1;
        case '9': out[0]=0x0E; out[1]=0x11; out[2]=0x11; out[3]=0x0F; out[4]=0x01; out[5]=0x01; out[6]=0x0E; return 1;
        default: return 0;
    }
}

void gfx_set_pixel(u8* target, s32 x, s32 y, u8 c) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
        return;
    }
    target[y * WIDTH + x] = c;
}

void gfx_fill_rect(u8* target, s32 x, s32 y, s32 w, s32 h, u8 c) {
    s32 yy;
    s32 xx;
    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            gfx_set_pixel(target, x + xx, y + yy, c);
        }
    }
}

void gfx_draw_text(u8* target, s32 x, s32 y, const char* s, u8 color) {
    s32 cx = x;
    while (*s) {
        u8 rows[7];
        if (glyph_for_char(*s, rows)) {
            draw_glyph_5x7_on(target, cx, y, rows, color);
        }
        cx += 6;
        s++;
    }
}

void gfx_draw_window(u8* target, const Window* w) {
    if (!w->visible) return;

    gfx_fill_rect(target, w->x, w->y, w->w, w->h, COL_WHITE);
    gfx_fill_rect(target, w->x, w->y, w->w, 1, COL_BLACK);
    gfx_fill_rect(target, w->x, w->y + w->h - 1, w->w, 1, COL_BLACK);
    gfx_fill_rect(target, w->x, w->y, 1, w->h, COL_BLACK);
    gfx_fill_rect(target, w->x + w->w - 1, w->y, 1, w->h, COL_BLACK);

    gfx_fill_rect(target, w->x + 1, w->y + 1, w->w - 2, 12, COL_GRAY_BAR);
    gfx_fill_rect(target, w->x + 1, w->y + 13, w->w - 2, 1, COL_BLACK);

    gfx_draw_text(target, w->x + 4, w->y + 3, w->title, COL_WHITE);

    gfx_fill_rect(target, w->x + w->w - 12, w->y + 2, 9, 9, COL_WHITE);
    gfx_fill_rect(target, w->x + w->w - 12, w->y + 2, 9, 1, COL_BLACK);
    gfx_fill_rect(target, w->x + w->w - 12, w->y + 10, 9, 1, COL_BLACK);
    gfx_fill_rect(target, w->x + w->w - 12, w->y + 2, 1, 9, COL_BLACK);
    gfx_fill_rect(target, w->x + w->w - 4, w->y + 2, 1, 9, COL_BLACK);
    gfx_draw_text(target, w->x + w->w - 10, w->y + 3, "X", COL_BLACK);
}

int gfx_window_hit_close(const Window* w, s32 mx, s32 my) {
    if (!w->visible) return 0;
    if (mx < w->x + w->w - 12 || mx > w->x + w->w - 4) return 0;
    if (my < w->y + 2 || my > w->y + 10) return 0;
    return 1;
}
