#include "window.h"

typedef unsigned short u16;
typedef unsigned int u32;

#define WIDTH 320
#define HEIGHT 200
#define FB ((volatile u8*)0xA0000)
#define CURSOR_W 8
#define CURSOR_H 12
#define DOCK_H 18

static Window calc_win = { 26, 24, 146, 164, "CALC", 0 };
static Window status_win = { 176, 24, 132, 92, "STATUS", 0 };
static Window about_win = { 86, 64, 148, 64, "ABOUT", 0 };
static Window virus_win = { 90, 40, 142, 82, "VIRUS", 0 };
static Window warn_win = { 56, 52, 208, 88, "WARNING", 0 };
static Window studio_win = { 44, 20, 232, 148, "STUDIO", 0 };
static Window explorer_win = { 38, 18, 246, 162, "EXPLORER", 0 };
static Window run_win = { 88, 72, 144, 56, "RUN", 0 };
static Window filedlg_win = { 60, 58, 200, 78, "FILE", 0 };

static s32 mouse_x = 160;
static s32 mouse_y = 100;
static s32 prev_mouse_x = 160;
static s32 prev_mouse_y = 100;

static u8 mouse_packet[3];
static u8 mouse_cycle = 0;
static u8 left_down = 0;
static u8 last_scancode = 0;

static u8 bg_buffer[WIDTH * HEIGHT];
static u8 cursor_under[CURSOR_W * CURSOR_H];
static Window* drag_win = 0;
static s32 drag_off_x = 0;
static s32 drag_off_y = 0;

static s32 calc_acc = 0;
static s32 calc_current = 0;
static char calc_op = 0;
static int calc_have_current = 0;
static int menu_open = 0;
static int dance_mode = 0;
static u32 dance_tick = 0;

#define MAX_CODE 384
static char studio_code[MAX_CODE] = "text+textbox=Hello World!";
static int studio_len = 24;
static char studio_preview[96] = "Hello World!";
static char filedlg_name[32] = "hello.mpp";
static int filedlg_len = 9;
static int filedlg_mode = 0; /* 0 none, 1 open, 2 save, 3 compile */

#define MAX_NAME 20
#define MAX_CONTENT 384
#define MAX_NODES 40
#define FS_MAGIC 0x58464D49u /* IMFX */
#define FS_VER 1u
#define FS_LBA_START 200u
#define FS_SECTORS 40u
typedef struct FsNode {
    int used;
    int is_dir;
    int parent;
    char name[MAX_NAME];
    char content[MAX_CONTENT];
} FsNode;

static FsNode fs[MAX_NODES];
static int fs_init_done = 0;
static int explorer_dir = 0;
static int explorer_sel = -1;
static int nav_back[16];
static int nav_back_len = 0;
static int nav_fwd[16];
static int nav_fwd_len = 0;
static int running_text_visible = 0;
static char running_text[96] = "";
static u8 fs_diskbuf[FS_SECTORS * 512];

static inline void outb(u16 port, u8 value) {
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(u16 port, u16 value) {
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 value;
    __asm__ __volatile__("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline u16 inw(u16 port) {
    u16 value;
    __asm__ __volatile__("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void io_wait(void) {
    outb(0x80, 0);
}

static void mem_copy(u8* dst, const u8* src, int n) {
    int i;
    for (i = 0; i < n; i++) dst[i] = src[i];
}

static void mem_set(u8* dst, u8 v, int n) {
    int i;
    for (i = 0; i < n; i++) dst[i] = v;
}

static void ata_wait_bsy(void) {
    while (inb(0x1F7) & 0x80) {}
}

static int ata_wait_drq(void) {
    u8 s;
    for (;;) {
        s = inb(0x1F7);
        if (s & 0x01) return 0;
        if (s & 0x08) return 1;
    }
}

static int ata_read_sector(u32 lba, u8* dst) {
    int i;
    ata_wait_bsy();
    outb(0x1F6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(0x1F1, 0);
    outb(0x1F2, 1);
    outb(0x1F3, (u8)(lba & 0xFF));
    outb(0x1F4, (u8)((lba >> 8) & 0xFF));
    outb(0x1F5, (u8)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x20);
    if (!ata_wait_drq()) return 0;
    for (i = 0; i < 256; i++) {
        u16 w = inw(0x1F0);
        dst[i * 2] = (u8)(w & 0xFF);
        dst[i * 2 + 1] = (u8)(w >> 8);
    }
    return 1;
}

static int ata_write_sector(u32 lba, const u8* src) {
    int i;
    ata_wait_bsy();
    outb(0x1F6, (u8)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(0x1F1, 0);
    outb(0x1F2, 1);
    outb(0x1F3, (u8)(lba & 0xFF));
    outb(0x1F4, (u8)((lba >> 8) & 0xFF));
    outb(0x1F5, (u8)((lba >> 16) & 0xFF));
    outb(0x1F7, 0x30);
    if (!ata_wait_drq()) return 0;
    for (i = 0; i < 256; i++) {
        u16 w = (u16)src[i * 2] | ((u16)src[i * 2 + 1] << 8);
        outw(0x1F0, w);
    }
    outb(0x1F7, 0xE7);
    ata_wait_bsy();
    return 1;
}

static void delay_loops(u32 loops) {
    volatile u32 i;
    for (i = 0; i < loops; i++) {
        io_wait();
    }
}

static void pit_set_frequency(u32 hz) {
    u32 divisor = 1193180 / hz;
    outb(0x43, 0xB6);
    outb(0x42, (u8)(divisor & 0xFF));
    outb(0x42, (u8)((divisor >> 8) & 0xFF));
}

static void speaker_on(void) {
    u8 tmp = inb(0x61);
    if ((tmp & 0x03) != 0x03) {
        outb(0x61, tmp | 0x03);
    }
}

static void speaker_off(void) {
    outb(0x61, inb(0x61) & 0xFC);
}

static void boot_beep(void) {
    pit_set_frequency(660);
    speaker_on();
    delay_loops(140000);
    speaker_off();
}

static void splash_wait_2s(void) {
    delay_loops(2800000);
}

static char hex_nibble(u8 x) {
    x &= 0x0F;
    return (x < 10) ? (char)('0' + x) : (char)('A' + (x - 10));
}

static void u8_to_hex(char* out, u8 v) {
    out[0] = hex_nibble((u8)(v >> 4));
    out[1] = hex_nibble(v);
    out[2] = 0;
}

static void i32_to_str(s32 value, char* out) {
    char buf[16];
    int i = 0;
    int j = 0;
    int neg = 0;
    u32 n;

    if (value == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    if (value < 0) {
        neg = 1;
        n = (u32)(-value);
    } else {
        n = (u32)value;
    }

    while (n > 0 && i < 15) {
        buf[i++] = (char)('0' + (n % 10));
        n /= 10;
    }

    if (neg) {
        out[j++] = '-';
    }

    while (i > 0) {
        out[j++] = buf[--i];
    }
    out[j] = 0;
}

static void blit_full_from_bg(void) {
    s32 i;
    for (i = 0; i < WIDTH * HEIGHT; i++) {
        FB[i] = bg_buffer[i];
    }
}

static void blit_shift_from_bg(s32 dx, s32 dy) {
    s32 y;
    s32 x;
    gfx_fill_rect((u8*)FB, 0, 0, WIDTH, HEIGHT, COL_GRAY_BG);
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            s32 sx = x - dx;
            s32 sy = y - dy;
            if (sx >= 0 && sx < WIDTH && sy >= 0 && sy < HEIGHT) {
                FB[y * WIDTH + x] = bg_buffer[sy * WIDTH + sx];
            }
        }
    }
}

static void save_under_cursor(s32 x, s32 y) {
    s32 yy;
    s32 xx;
    for (yy = 0; yy < CURSOR_H; yy++) {
        for (xx = 0; xx < CURSOR_W; xx++) {
            cursor_under[yy * CURSOR_W + xx] = FB[(y + yy) * WIDTH + (x + xx)];
        }
    }
}

static void restore_under_cursor(s32 x, s32 y) {
    s32 yy;
    s32 xx;
    for (yy = 0; yy < CURSOR_H; yy++) {
        for (xx = 0; xx < CURSOR_W; xx++) {
            FB[(y + yy) * WIDTH + (x + xx)] = cursor_under[yy * CURSOR_W + xx];
        }
    }
}

static void draw_mouse_cursor(s32 x, s32 y) {
    s32 yy;
    s32 xx;
    save_under_cursor(x, y);

    for (yy = 0; yy < CURSOR_H; yy++) {
        for (xx = 0; xx < CURSOR_W; xx++) {
            u8 c = COL_WHITE;
            if (yy == 0 || yy == (CURSOR_H - 1) || xx == 0 || xx == (CURSOR_W - 1)) {
                c = COL_BLACK;
            }
            FB[(y + yy) * WIDTH + (x + xx)] = c;
        }
    }
}

static int point_in_rect(s32 px, s32 py, s32 x, s32 y, s32 w, s32 h) {
    return (px >= x && px < (x + w) && py >= y && py < (y + h));
}

static int str_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int str_ends_with(const char* s, const char* suf) {
    int sl = str_len(s);
    int pl = str_len(suf);
    int i;
    if (pl > sl) return 0;
    for (i = 0; i < pl; i++) {
        if (s[sl - pl + i] != suf[i]) return 0;
    }
    return 1;
}

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    if (max <= 0) return;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void parse_mpp_preview(const char* code, char* out, int out_max) {
    int i = 0;
    int eq = -1;
    int start;
    int j = 0;
    while (code[i]) {
        if (code[i] == '=') eq = i;
        i++;
    }
    if (eq < 0) {
        str_copy(out, "INVALID M++", out_max);
        return;
    }
    start = eq + 1;
    while (code[start] == ' ') start++;
    if (code[start] == '\"') start++;
    while (code[start] && code[start] != '\"' && j < out_max - 1) {
        out[j++] = code[start++];
    }
    out[j] = 0;
    if (j == 0) str_copy(out, "INVALID M++", out_max);
}

static void fs_clear(void) {
    int i;
    for (i = 0; i < MAX_NODES; i++) {
        fs[i].used = 0;
        fs[i].is_dir = 0;
        fs[i].parent = -1;
        fs[i].name[0] = 0;
        fs[i].content[0] = 0;
    }
}

static int fs_alloc_node(void) {
    int i;
    for (i = 0; i < MAX_NODES; i++) {
        if (!fs[i].used) {
            fs[i].used = 1;
            return i;
        }
    }
    return -1;
}

static int fs_add_dir(int parent, const char* name) {
    int id = fs_alloc_node();
    if (id < 0) return -1;
    fs[id].is_dir = 1;
    fs[id].parent = parent;
    str_copy(fs[id].name, name, MAX_NAME);
    return id;
}

static int fs_add_file(int parent, const char* name, const char* content) {
    int id = fs_alloc_node();
    if (id < 0) return -1;
    fs[id].is_dir = 0;
    fs[id].parent = parent;
    str_copy(fs[id].name, name, MAX_NAME);
    str_copy(fs[id].content, content, MAX_CONTENT);
    return id;
}

static int fs_load_from_disk(void) {
    u32 magic;
    u32 ver;
    int i;
    for (i = 0; i < (int)FS_SECTORS; i++) {
        if (!ata_read_sector(FS_LBA_START + (u32)i, fs_diskbuf + i * 512)) return 0;
    }
    magic = (u32)fs_diskbuf[0] | ((u32)fs_diskbuf[1] << 8) | ((u32)fs_diskbuf[2] << 16) | ((u32)fs_diskbuf[3] << 24);
    ver = (u32)fs_diskbuf[4] | ((u32)fs_diskbuf[5] << 8) | ((u32)fs_diskbuf[6] << 16) | ((u32)fs_diskbuf[7] << 24);
    if (magic != FS_MAGIC || ver != FS_VER) return 0;
    mem_copy((u8*)fs, fs_diskbuf + 8, MAX_NODES * (int)sizeof(FsNode));
    return 1;
}

static void fs_save_to_disk(void) {
    int i;
    mem_set(fs_diskbuf, 0, (int)(FS_SECTORS * 512));
    fs_diskbuf[0] = (u8)(FS_MAGIC & 0xFF);
    fs_diskbuf[1] = (u8)((FS_MAGIC >> 8) & 0xFF);
    fs_diskbuf[2] = (u8)((FS_MAGIC >> 16) & 0xFF);
    fs_diskbuf[3] = (u8)((FS_MAGIC >> 24) & 0xFF);
    fs_diskbuf[4] = (u8)(FS_VER & 0xFF);
    fs_diskbuf[5] = (u8)((FS_VER >> 8) & 0xFF);
    fs_diskbuf[6] = (u8)((FS_VER >> 16) & 0xFF);
    fs_diskbuf[7] = (u8)((FS_VER >> 24) & 0xFF);
    mem_copy(fs_diskbuf + 8, (const u8*)fs, MAX_NODES * (int)sizeof(FsNode));
    for (i = 0; i < (int)FS_SECTORS; i++) {
        if (!ata_write_sector(FS_LBA_START + (u32)i, fs_diskbuf + i * 512)) break;
    }
}

static void fs_init(void) {
    int apps;
    if (fs_init_done) return;
    if (!fs_load_from_disk()) {
        fs_clear();
        fs[0].used = 1;
        fs[0].is_dir = 1;
        fs[0].parent = -1;
        str_copy(fs[0].name, "HD", MAX_NAME);
        apps = fs_add_dir(0, "apps");
        fs_add_file(apps, "hello.mpp", "text+textbox=Hello World!");
        fs_add_file(apps, "demo.mex", "text+textbox=Hello from MEX");
        fs_save_to_disk();
    }
    fs_init_done = 1;
}

static int fs_child_by_index(int dir, int idx) {
    int i;
    int n = 0;
    for (i = 0; i < MAX_NODES; i++) {
        if (fs[i].used && fs[i].parent == dir) {
            if (n == idx) return i;
            n++;
        }
    }
    return -1;
}

static s32 cursor_hit_x(void) {
    return mouse_x + (CURSOR_W / 2);
}

static s32 cursor_hit_y(void) {
    return mouse_y + (CURSOR_H / 2);
}

static void clamp_window(Window* w) {
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
    if (w->x + w->w > WIDTH) w->x = WIDTH - w->w;
    if (w->y + w->h > HEIGHT - DOCK_H) w->y = HEIGHT - DOCK_H - w->h;
    if (w->x < 0) w->x = 0;
    if (w->y < 0) w->y = 0;
}

static int hit_titlebar(Window* w, s32 mx, s32 my) {
    if (!w->visible) return 0;
    if (!point_in_rect(mx, my, w->x + 1, w->y + 1, w->w - 14, 12)) return 0;
    return 1;
}

static int hit_window_body(Window* w, s32 mx, s32 my) {
    if (!w->visible) return 0;
    return point_in_rect(mx, my, w->x, w->y, w->w, w->h);
}

static int start_drag_if_hit(s32 mx, s32 my) {
    if (hit_titlebar(&filedlg_win, mx, my)) {
        drag_win = &filedlg_win;
    } else if (hit_titlebar(&warn_win, mx, my)) {
        drag_win = &warn_win;
    } else if (hit_titlebar(&explorer_win, mx, my)) {
        drag_win = &explorer_win;
    } else if (hit_titlebar(&studio_win, mx, my)) {
        drag_win = &studio_win;
    } else if (hit_titlebar(&run_win, mx, my)) {
        drag_win = &run_win;
    } else if (hit_titlebar(&virus_win, mx, my)) {
        drag_win = &virus_win;
    } else if (hit_titlebar(&about_win, mx, my)) {
        drag_win = &about_win;
    } else if (hit_titlebar(&status_win, mx, my)) {
        drag_win = &status_win;
    } else if (hit_titlebar(&calc_win, mx, my)) {
        drag_win = &calc_win;
    } else {
        return 0;
    }

    drag_off_x = mx - drag_win->x;
    drag_off_y = my - drag_win->y;
    return 1;
}

static void calc_clear(void) {
    calc_acc = 0;
    calc_current = 0;
    calc_op = 0;
    calc_have_current = 0;
}

static void calc_apply_pending(void) {
    if (!calc_have_current) return;

    if (calc_op == 0) {
        calc_acc = calc_current;
    } else if (calc_op == '+') {
        calc_acc += calc_current;
    } else if (calc_op == '-') {
        calc_acc -= calc_current;
    } else if (calc_op == 'x') {
        calc_acc *= calc_current;
    }

    calc_current = 0;
    calc_have_current = 0;
}

static void calc_press(char key) {
    if (key >= '0' && key <= '9') {
        if (!calc_have_current) {
            calc_current = 0;
            calc_have_current = 1;
        }
        calc_current = calc_current * 10 + (key - '0');
        return;
    }

    if (key == 'C') {
        calc_clear();
        return;
    }

    if (key == '+' || key == '-' || key == 'x') {
        calc_apply_pending();
        calc_op = key;
        return;
    }

    if (key == '=') {
        calc_apply_pending();
        calc_op = 0;
        calc_current = calc_acc;
        calc_have_current = 1;
    }
}

static char scancode_to_calc_key(u8 sc) {
    switch (sc) {
        case 0x0B: return '0';
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0C: return '-';
        case 0x1C: return '=';
        case 0x2D: return 'x';
        case 0x2E: return 'C';
        case 0x4E: return '+';
        default: return 0;
    }
}

static char scancode_to_ascii(u8 sc) {
    switch (sc) {
        case 0x39: return ' ';
        case 0x0E: return '\b';
        case 0x1C: return '\n';
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2B: return '\\';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x10: return 'q'; case 0x11: return 'w'; case 0x12: return 'e'; case 0x13: return 'r';
        case 0x14: return 't'; case 0x15: return 'y'; case 0x16: return 'u'; case 0x17: return 'i';
        case 0x18: return 'o'; case 0x19: return 'p'; case 0x1E: return 'a'; case 0x1F: return 's';
        case 0x20: return 'd'; case 0x21: return 'f'; case 0x22: return 'g'; case 0x23: return 'h';
        case 0x24: return 'j'; case 0x25: return 'k'; case 0x26: return 'l'; case 0x2C: return 'z';
        case 0x2D: return 'x'; case 0x2E: return 'c'; case 0x2F: return 'v'; case 0x30: return 'b';
        case 0x31: return 'n'; case 0x32: return 'm';
        default: return 0;
    }
}

static void draw_icon_calc(s32 x, s32 y) {
    gfx_fill_rect(bg_buffer, x, y, 24, 24, COL_WHITE);
    gfx_fill_rect(bg_buffer, x, y, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y + 23, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 23, y, 1, 24, COL_BLACK);
    gfx_draw_text(bg_buffer, x + 7, y + 9, "+", COL_BLACK);
    gfx_draw_text(bg_buffer, x + 13, y + 9, "-", COL_BLACK);
    gfx_draw_text(bg_buffer, x + 7, y + 15, "x", COL_BLACK);
}

static void draw_icon_status(s32 x, s32 y) {
    gfx_fill_rect(bg_buffer, x, y, 24, 24, COL_WHITE);
    gfx_fill_rect(bg_buffer, x, y, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y + 23, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 23, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 4, y + 15, 4, 5, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 10, y + 11, 4, 9, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 16, y + 7, 4, 13, COL_BLACK);
}

static void draw_icon_studio(s32 x, s32 y) {
    gfx_fill_rect(bg_buffer, x, y, 24, 24, COL_WHITE);
    gfx_fill_rect(bg_buffer, x, y, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y + 23, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 23, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 4, y + 5, 16, 12, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, x + 7, y + 18, "M", COL_BLACK);
}

static void draw_icon_explorer(s32 x, s32 y) {
    gfx_fill_rect(bg_buffer, x, y, 24, 24, COL_WHITE);
    gfx_fill_rect(bg_buffer, x, y, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y + 23, 24, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, x, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 23, y, 1, 24, COL_BLACK);
    gfx_fill_rect(bg_buffer, x + 3, y + 8, 18, 10, COL_GRAY_BG);
    gfx_fill_rect(bg_buffer, x + 3, y + 6, 8, 3, COL_GRAY_BG);
}

static void draw_dock(void) {
    gfx_fill_rect(bg_buffer, 0, HEIGHT - DOCK_H, WIDTH, DOCK_H, COL_GRAY_BAR);
    gfx_fill_rect(bg_buffer, 0, HEIGHT - DOCK_H, WIDTH, 1, COL_BLACK);

    gfx_fill_rect(bg_buffer, 6, HEIGHT - 15, 74, 12, COL_WHITE);
    gfx_fill_rect(bg_buffer, 6, HEIGHT - 15, 74, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, 6, HEIGHT - 4, 74, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, 6, HEIGHT - 15, 1, 12, COL_BLACK);
    gfx_fill_rect(bg_buffer, 79, HEIGHT - 15, 1, 12, COL_BLACK);
    gfx_draw_text(bg_buffer, 11, HEIGHT - 12, "Inter-Minux", COL_BLACK);

    if (menu_open) {
        gfx_fill_rect(bg_buffer, 6, HEIGHT - 63, 102, 44, COL_WHITE);
        gfx_fill_rect(bg_buffer, 6, HEIGHT - 63, 102, 1, COL_BLACK);
        gfx_fill_rect(bg_buffer, 6, HEIGHT - 20, 102, 1, COL_BLACK);
        gfx_fill_rect(bg_buffer, 6, HEIGHT - 63, 1, 44, COL_BLACK);
        gfx_fill_rect(bg_buffer, 107, HEIGHT - 63, 1, 44, COL_BLACK);
        gfx_draw_text(bg_buffer, 12, HEIGHT - 55, "ABOUT", COL_BLACK);
        gfx_draw_text(bg_buffer, 12, HEIGHT - 41, "VIRUS PANEL", COL_BLACK);
    }

}

static void draw_calc_buttons(void) {
    const char labels[16] = {
        '7','8','9','+',
        '4','5','6','-',
        '1','2','3','x',
        'C','0','=','='
    };
    s32 i;

    for (i = 0; i < 16; i++) {
        s32 col = i % 4;
        s32 row = i / 4;
        s32 bx = calc_win.x + 8 + col * 33;
        s32 by = calc_win.y + 52 + row * 24;
        char s[2];
        s[0] = labels[i];
        s[1] = 0;

        gfx_fill_rect(bg_buffer, bx, by, 28, 20, COL_GRAY_BG);
        gfx_fill_rect(bg_buffer, bx, by, 28, 1, COL_BLACK);
        gfx_fill_rect(bg_buffer, bx, by + 19, 28, 1, COL_BLACK);
        gfx_fill_rect(bg_buffer, bx, by, 1, 20, COL_BLACK);
        gfx_fill_rect(bg_buffer, bx + 27, by, 1, 20, COL_BLACK);
        gfx_draw_text(bg_buffer, bx + 10, by + 6, s, COL_BLACK);
    }
}

static void draw_calc_app(void) {
    char num[16];
    char expr[20];

    if (!calc_win.visible) return;

    gfx_fill_rect(bg_buffer, calc_win.x + 6, calc_win.y + 18, calc_win.w - 12, 22, COL_GRAY_BG);
    gfx_fill_rect(bg_buffer, calc_win.x + 6, calc_win.y + 18, calc_win.w - 12, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, calc_win.x + 6, calc_win.y + 39, calc_win.w - 12, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, calc_win.x + 6, calc_win.y + 18, 1, 22, COL_BLACK);
    gfx_fill_rect(bg_buffer, calc_win.x + calc_win.w - 7, calc_win.y + 18, 1, 22, COL_BLACK);

    i32_to_str(calc_have_current ? calc_current : calc_acc, num);

    expr[0] = 'O'; expr[1] = 'P'; expr[2] = ':'; expr[3] = ' ';
    expr[4] = calc_op ? calc_op : '-';
    expr[5] = 0;

    gfx_draw_text(bg_buffer, calc_win.x + 10, calc_win.y + 23, num, COL_BLACK);
    gfx_draw_text(bg_buffer, calc_win.x + 90, calc_win.y + 23, expr, COL_BLACK);

    draw_calc_buttons();
}

static void draw_status_app(void) {
    char keyhex[3];
    char line1[18];
    char line2[18];

    if (!status_win.visible) return;

    gfx_fill_rect(bg_buffer, status_win.x + 6, status_win.y + 18, status_win.w - 12, status_win.h - 24, COL_GRAY_BG);

    line1[0]='M'; line1[1]='O'; line1[2]='U'; line1[3]='S'; line1[4]='E'; line1[5]=':'; line1[6]=' ';
    line1[7]='L'; line1[8]='='; line1[9]=(left_down ? '1' : '0'); line1[10]=0;

    u8_to_hex(keyhex, last_scancode);
    line2[0]='K'; line2[1]='E'; line2[2]='Y'; line2[3]=':'; line2[4]=' ';
    line2[5]=keyhex[0]; line2[6]=keyhex[1]; line2[7]=0;

    gfx_draw_text(bg_buffer, status_win.x + 10, status_win.y + 28, line1, COL_BLACK);
    gfx_draw_text(bg_buffer, status_win.x + 10, status_win.y + 40, line2, COL_BLACK);
}

static void draw_about_app(void) {
    if (!about_win.visible) return;

    gfx_fill_rect(bg_buffer, about_win.x + 6, about_win.y + 20, about_win.w - 12, about_win.h - 26, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, about_win.x + 16, about_win.y + 30, "Inter-Minux", COL_BLACK);
    gfx_draw_text(bg_buffer, about_win.x + 16, about_win.y + 42, "VERSION 1.0", COL_BLACK);
}

static void draw_virus_app(void) {
    if (!virus_win.visible) return;

    gfx_fill_rect(bg_buffer, virus_win.x + 6, virus_win.y + 20, virus_win.w - 12, virus_win.h - 26, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, virus_win.x + 14, virus_win.y + 28, "HARMLESS DEMO VIRUS", COL_BLACK);

    gfx_fill_rect(bg_buffer, virus_win.x + 20, virus_win.y + 46, 90, 20, COL_WHITE);
    gfx_fill_rect(bg_buffer, virus_win.x + 20, virus_win.y + 46, 90, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, virus_win.x + 20, virus_win.y + 65, 90, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, virus_win.x + 20, virus_win.y + 46, 1, 20, COL_BLACK);
    gfx_fill_rect(bg_buffer, virus_win.x + 109, virus_win.y + 46, 1, 20, COL_BLACK);
    gfx_draw_text(bg_buffer, virus_win.x + 43, virus_win.y + 53, "VIRUS 1", COL_BLACK);
}

static void draw_warning_dialog(void) {
    if (!warn_win.visible) return;

    gfx_fill_rect(bg_buffer, warn_win.x + 6, warn_win.y + 20, warn_win.w - 12, warn_win.h - 26, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, warn_win.x + 12, warn_win.y + 30, "THIS IS A HARMLESS VIRUS", COL_BLACK);
    gfx_draw_text(bg_buffer, warn_win.x + 12, warn_win.y + 42, "NEEDS RESTART TO STOP", COL_BLACK);
    gfx_draw_text(bg_buffer, warn_win.x + 12, warn_win.y + 54, "ARE YOU SURE", COL_BLACK);

    gfx_fill_rect(bg_buffer, warn_win.x + 22, warn_win.y + 66, 58, 16, COL_WHITE);
    gfx_fill_rect(bg_buffer, warn_win.x + 22, warn_win.y + 66, 58, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 22, warn_win.y + 81, 58, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 22, warn_win.y + 66, 1, 16, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 79, warn_win.y + 66, 1, 16, COL_BLACK);
    gfx_draw_text(bg_buffer, warn_win.x + 40, warn_win.y + 70, "YES", COL_BLACK);

    gfx_fill_rect(bg_buffer, warn_win.x + 126, warn_win.y + 66, 58, 16, COL_WHITE);
    gfx_fill_rect(bg_buffer, warn_win.x + 126, warn_win.y + 66, 58, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 126, warn_win.y + 81, 58, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 126, warn_win.y + 66, 1, 16, COL_BLACK);
    gfx_fill_rect(bg_buffer, warn_win.x + 183, warn_win.y + 66, 1, 16, COL_BLACK);
    gfx_draw_text(bg_buffer, warn_win.x + 147, warn_win.y + 70, "NO", COL_BLACK);
}

static void draw_studio_app(void) {
    if (!studio_win.visible) return;
    parse_mpp_preview(studio_code, studio_preview, 96);

    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 20, studio_win.w - 12, 14, COL_GRAY_BG);
    gfx_fill_rect(bg_buffer, studio_win.x + 8, studio_win.y + 22, 34, 10, COL_WHITE);
    gfx_fill_rect(bg_buffer, studio_win.x + 46, studio_win.y + 22, 34, 10, COL_WHITE);
    gfx_fill_rect(bg_buffer, studio_win.x + 84, studio_win.y + 22, 48, 10, COL_WHITE);
    gfx_fill_rect(bg_buffer, studio_win.x + 136, studio_win.y + 22, 56, 10, COL_WHITE);
    gfx_draw_text(bg_buffer, studio_win.x + 11, studio_win.y + 24, "OPEN", COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 50, studio_win.y + 24, "SAVE", COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 88, studio_win.y + 24, "COMPILE", COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 140, studio_win.y + 24, "EXPLORER", COL_BLACK);

    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 38, studio_win.w - 12, 54, COL_WHITE);
    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 38, studio_win.w - 12, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 91, studio_win.w - 12, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 38, 1, 54, COL_BLACK);
    gfx_fill_rect(bg_buffer, studio_win.x + studio_win.w - 7, studio_win.y + 38, 1, 54, COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 10, studio_win.y + 42, studio_code, COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 10 + studio_len * 6, studio_win.y + 42, "I", COL_BLACK);

    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 98, studio_win.w - 12, 40, COL_GRAY_BG);
    gfx_fill_rect(bg_buffer, studio_win.x + 6, studio_win.y + 98, studio_win.w - 12, 1, COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 10, studio_win.y + 104, "PREVIEW:", COL_BLACK);
    gfx_draw_text(bg_buffer, studio_win.x + 10, studio_win.y + 116, studio_preview, COL_BLACK);
}

static void draw_explorer_app(void) {
    int i;
    if (!explorer_win.visible) return;
    gfx_fill_rect(bg_buffer, explorer_win.x + 6, explorer_win.y + 20, explorer_win.w - 12, explorer_win.h - 26, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, explorer_win.x + 10, explorer_win.y + 24, "BACK FWD NEWF RENAME", COL_BLACK);

    for (i = 0; i < 8; i++) {
        int id = fs_child_by_index(explorer_dir, i);
        int y = explorer_win.y + 42 + i * 12;
        if (id < 0) break;
        if (i == explorer_sel) {
            gfx_fill_rect(bg_buffer, explorer_win.x + 8, y - 1, explorer_win.w - 16, 10, COL_WHITE);
        }
        gfx_draw_text(bg_buffer, explorer_win.x + 10, y, fs[id].name, COL_BLACK);
        if (fs[id].is_dir) {
            gfx_draw_text(bg_buffer, explorer_win.x + explorer_win.w - 30, y, "/", COL_BLACK);
        }
    }
}

static void draw_run_app(void) {
    if (!run_win.visible) return;
    gfx_fill_rect(bg_buffer, run_win.x + 6, run_win.y + 20, run_win.w - 12, run_win.h - 26, COL_GRAY_BG);
    if (running_text_visible) {
        gfx_draw_text(bg_buffer, run_win.x + 10, run_win.y + 32, running_text, COL_BLACK);
    }
}

static void draw_filedlg_app(void) {
    if (!filedlg_win.visible) return;
    gfx_fill_rect(bg_buffer, filedlg_win.x + 6, filedlg_win.y + 20, filedlg_win.w - 12, filedlg_win.h - 26, COL_GRAY_BG);
    if (filedlg_mode == 1) gfx_draw_text(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 26, "OPEN FILE:", COL_BLACK);
    if (filedlg_mode == 2) gfx_draw_text(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 26, "SAVE AS FILE:", COL_BLACK);
    if (filedlg_mode == 3) {
        gfx_draw_text(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 26, "COMPILE TO FILE:", COL_BLACK);
        gfx_draw_text(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 36, ".MEX REQUIRED", COL_BLACK);
    }
    gfx_fill_rect(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 48, filedlg_win.w - 20, 14, COL_WHITE);
    gfx_fill_rect(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 48, filedlg_win.w - 20, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 61, filedlg_win.w - 20, 1, COL_BLACK);
    gfx_fill_rect(bg_buffer, filedlg_win.x + 10, filedlg_win.y + 48, 1, 14, COL_BLACK);
    gfx_fill_rect(bg_buffer, filedlg_win.x + filedlg_win.w - 11, filedlg_win.y + 48, 1, 14, COL_BLACK);
    gfx_draw_text(bg_buffer, filedlg_win.x + 14, filedlg_win.y + 51, filedlg_name, COL_BLACK);
    gfx_draw_text(bg_buffer, filedlg_win.x + 14 + filedlg_len * 6, filedlg_win.y + 51, "I", COL_BLACK);
}

static void draw_window_title_overlays(void) {
    if (calc_win.visible) {
        gfx_draw_text(bg_buffer, calc_win.x + 4, calc_win.y + 3, "CALC", COL_WHITE);
    }
    if (status_win.visible) {
        gfx_draw_text(bg_buffer, status_win.x + 4, status_win.y + 3, "STATUS", COL_WHITE);
    }
    if (about_win.visible) {
        gfx_draw_text(bg_buffer, about_win.x + 4, about_win.y + 3, "ABOUT", COL_WHITE);
    }
    if (virus_win.visible) {
        gfx_draw_text(bg_buffer, virus_win.x + 4, virus_win.y + 3, "VIRUS", COL_WHITE);
    }
    if (warn_win.visible) {
        gfx_draw_text(bg_buffer, warn_win.x + 4, warn_win.y + 3, "WARNING", COL_WHITE);
    }
    if (studio_win.visible) {
        gfx_draw_text(bg_buffer, studio_win.x + 4, studio_win.y + 3, "STUDIO", COL_WHITE);
    }
    if (explorer_win.visible) {
        gfx_draw_text(bg_buffer, explorer_win.x + 4, explorer_win.y + 3, "EXPLORER", COL_WHITE);
    }
    if (run_win.visible) {
        gfx_draw_text(bg_buffer, run_win.x + 4, run_win.y + 3, "RUN", COL_WHITE);
    }
    if (filedlg_win.visible) {
        if (filedlg_mode == 1) gfx_draw_text(bg_buffer, filedlg_win.x + 4, filedlg_win.y + 3, "OPEN", COL_WHITE);
        if (filedlg_mode == 2) gfx_draw_text(bg_buffer, filedlg_win.x + 4, filedlg_win.y + 3, "SAVE AS", COL_WHITE);
        if (filedlg_mode == 3) gfx_draw_text(bg_buffer, filedlg_win.x + 4, filedlg_win.y + 3, "COMPILE", COL_WHITE);
    }
}

static void draw_desktop_icons(void) {
    draw_icon_calc(14, 18);
    gfx_draw_text(bg_buffer, 12, 45, "CALC", COL_BLACK);

    draw_icon_status(14, 62);
    gfx_draw_text(bg_buffer, 10, 89, "STATUS", COL_BLACK);

    draw_icon_studio(14, 106);
    gfx_draw_text(bg_buffer, 8, 133, "STUDIO", COL_BLACK);

    draw_icon_explorer(14, 138);
    gfx_draw_text(bg_buffer, 2, 165, "EXPLORER", COL_BLACK);
}

static void draw_desktop_to_bg(void) {
    gfx_fill_rect(bg_buffer, 0, 0, WIDTH, HEIGHT, COL_GRAY_BG);

    draw_desktop_icons();

    gfx_draw_window(bg_buffer, &calc_win);
    gfx_draw_window(bg_buffer, &status_win);
    gfx_draw_window(bg_buffer, &about_win);
    gfx_draw_window(bg_buffer, &virus_win);
    gfx_draw_window(bg_buffer, &warn_win);
    gfx_draw_window(bg_buffer, &studio_win);
    gfx_draw_window(bg_buffer, &explorer_win);
    gfx_draw_window(bg_buffer, &run_win);
    gfx_draw_window(bg_buffer, &filedlg_win);
    draw_window_title_overlays();

    draw_calc_app();
    draw_status_app();
    draw_about_app();
    draw_virus_app();
    draw_warning_dialog();
    draw_studio_app();
    draw_explorer_app();
    draw_run_app();
    draw_filedlg_app();

    draw_dock();
}

static void draw_splash_to_bg(void) {
    Window splash = { 68, 68, 184, 62, "Inter-Minux", 1 };
    gfx_fill_rect(bg_buffer, 0, 0, WIDTH, HEIGHT, COL_GRAY_BG);
    gfx_draw_window(bg_buffer, &splash);
    gfx_fill_rect(bg_buffer, splash.x + 6, splash.y + 20, splash.w - 12, splash.h - 26, COL_GRAY_BG);
    gfx_draw_text(bg_buffer, splash.x + 18, splash.y + 30, "Inter-Minux 1.0", COL_BLACK);
}

static int handle_calc_click(s32 mx, s32 my) {
    const char labels[16] = {
        '7','8','9','+',
        '4','5','6','-',
        '1','2','3','x',
        'C','0','=','='
    };
    s32 i;

    if (!calc_win.visible) return 0;

    for (i = 0; i < 16; i++) {
        s32 col = i % 4;
        s32 row = i / 4;
        s32 bx = calc_win.x + 8 + col * 33;
        s32 by = calc_win.y + 52 + row * 24;
        if (point_in_rect(mx, my, bx, by, 28, 20)) {
            calc_press(labels[i]);
            return 1;
        }
    }

    return 0;
}

static void nav_push_back(int dir) {
    if (nav_back_len < 16) nav_back[nav_back_len++] = dir;
}

static void nav_push_fwd(int dir) {
    if (nav_fwd_len < 16) nav_fwd[nav_fwd_len++] = dir;
}

static void explorer_open_dir(int dir) {
    if (dir < 0 || !fs[dir].used || !fs[dir].is_dir) return;
    nav_push_back(explorer_dir);
    explorer_dir = dir;
    explorer_sel = -1;
    nav_fwd_len = 0;
}

static void run_mex_text(const char* content) {
    parse_mpp_preview(content, running_text, 96);
    run_win.visible = 1;
    running_text_visible = 1;
}

static int fs_find_in_dir(int dir, const char* name) {
    int i;
    for (i = 0; i < MAX_NODES; i++) {
        if (fs[i].used && fs[i].parent == dir && str_len(fs[i].name) == str_len(name)) {
            int j = 0;
            int ok = 1;
            while (name[j]) {
                if (fs[i].name[j] != name[j]) { ok = 0; break; }
                j++;
            }
            if (ok) return i;
        }
    }
    return -1;
}

static void ensure_ext(char* name, int max, const char* ext) {
    int n = str_len(name);
    int e = str_len(ext);
    int i;
    if (n + e >= max) return;
    if (str_ends_with(name, ext)) return;
    for (i = 0; i < e; i++) {
        name[n + i] = ext[i];
    }
    name[n + e] = 0;
}

static void filedlg_execute(void) {
    int id;
    if (filedlg_mode == 1) {
        id = fs_find_in_dir(explorer_dir, filedlg_name);
        if (id >= 0 && !fs[id].is_dir) {
            if (str_ends_with(fs[id].name, ".mpp")) {
                str_copy(studio_code, fs[id].content, MAX_CODE);
                studio_len = str_len(studio_code);
                studio_win.visible = 1;
            } else if (str_ends_with(fs[id].name, ".mex")) {
                run_mex_text(fs[id].content);
            }
        }
    } else if (filedlg_mode == 2) {
        ensure_ext(filedlg_name, 32, ".mpp");
        id = fs_find_in_dir(explorer_dir, filedlg_name);
        if (id >= 0 && !fs[id].is_dir) {
            str_copy(fs[id].content, studio_code, MAX_CONTENT);
        } else {
            fs_add_file(explorer_dir, filedlg_name, studio_code);
        }
        fs_save_to_disk();
    } else if (filedlg_mode == 3) {
        ensure_ext(filedlg_name, 32, ".mex");
        id = fs_find_in_dir(explorer_dir, filedlg_name);
        if (id >= 0 && !fs[id].is_dir) {
            str_copy(fs[id].content, studio_code, MAX_CONTENT);
        } else {
            fs_add_file(explorer_dir, filedlg_name, studio_code);
        }
        fs_save_to_disk();
    }
    filedlg_mode = 0;
    filedlg_win.visible = 0;
}

static int handle_ui_click(s32 mx, s32 my) {
    int in_any_window;
    if (gfx_window_hit_close(&calc_win, mx, my)) {
        calc_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&status_win, mx, my)) {
        status_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&about_win, mx, my)) {
        about_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&virus_win, mx, my)) {
        virus_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&warn_win, mx, my)) {
        warn_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&studio_win, mx, my)) {
        studio_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&explorer_win, mx, my)) {
        explorer_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&run_win, mx, my)) {
        run_win.visible = 0;
        return 1;
    }
    if (gfx_window_hit_close(&filedlg_win, mx, my)) {
        filedlg_win.visible = 0;
        filedlg_mode = 0;
        return 1;
    }

    in_any_window =
        hit_window_body(&calc_win, mx, my) ||
        hit_window_body(&status_win, mx, my) ||
        hit_window_body(&about_win, mx, my) ||
        hit_window_body(&virus_win, mx, my) ||
        hit_window_body(&warn_win, mx, my) ||
        hit_window_body(&studio_win, mx, my) ||
        hit_window_body(&explorer_win, mx, my) ||
        hit_window_body(&run_win, mx, my) ||
        hit_window_body(&filedlg_win, mx, my);

    if (!in_any_window) {
        if (point_in_rect(mx, my, 14, 18, 24, 24)) {
            calc_win.visible = 1;
            return 1;
        }
        if (point_in_rect(mx, my, 14, 62, 24, 24)) {
            status_win.visible = 1;
            return 1;
        }
        if (point_in_rect(mx, my, 14, 106, 24, 24)) {
            studio_win.visible = 1;
            return 1;
        }
        if (point_in_rect(mx, my, 14, 138, 24, 24)) {
            explorer_win.visible = 1;
            return 1;
        }
    }

    if (point_in_rect(mx, my, 6, HEIGHT - 15, 74, 12)) {
        menu_open = !menu_open;
        return 1;
    }

    if (studio_win.visible && point_in_rect(mx, my, studio_win.x + 8, studio_win.y + 22, 34, 10)) {
        filedlg_mode = 1;
        filedlg_win.visible = 1;
        str_copy(filedlg_name, "hello.mpp", 32);
        filedlg_len = str_len(filedlg_name);
        return 1;
    }
    if (studio_win.visible && point_in_rect(mx, my, studio_win.x + 46, studio_win.y + 22, 34, 10)) {
        filedlg_mode = 2;
        filedlg_win.visible = 1;
        str_copy(filedlg_name, "hello.mpp", 32);
        filedlg_len = str_len(filedlg_name);
        return 1;
    }
    if (studio_win.visible && point_in_rect(mx, my, studio_win.x + 84, studio_win.y + 22, 48, 10)) {
        filedlg_mode = 3;
        filedlg_win.visible = 1;
        str_copy(filedlg_name, "hello.mex", 32);
        filedlg_len = str_len(filedlg_name);
        return 1;
    }
    if (studio_win.visible && point_in_rect(mx, my, studio_win.x + 136, studio_win.y + 22, 56, 10)) {
        explorer_win.visible = 1;
        return 1;
    }

    if (menu_open && point_in_rect(mx, my, 6, HEIGHT - 63, 102, 20)) {
        about_win.visible = 1;
        menu_open = 0;
        return 1;
    }
    if (menu_open && point_in_rect(mx, my, 6, HEIGHT - 43, 102, 24)) {
        virus_win.visible = 1;
        menu_open = 0;
        return 1;
    }

    if (menu_open) {
        menu_open = 0;
        return 1;
    }

    if (handle_calc_click(mx, my)) {
        return 1;
    }

    if (explorer_win.visible && point_in_rect(mx, my, explorer_win.x + 8, explorer_win.y + 24, 26, 10)) {
        if (nav_back_len > 0) {
            nav_push_fwd(explorer_dir);
            explorer_dir = nav_back[--nav_back_len];
            explorer_sel = -1;
        }
        return 1;
    }
    if (explorer_win.visible && point_in_rect(mx, my, explorer_win.x + 38, explorer_win.y + 24, 24, 10)) {
        if (nav_fwd_len > 0) {
            nav_push_back(explorer_dir);
            explorer_dir = nav_fwd[--nav_fwd_len];
            explorer_sel = -1;
        }
        return 1;
    }
    if (explorer_win.visible && point_in_rect(mx, my, explorer_win.x + 66, explorer_win.y + 24, 28, 10)) {
        int nd = fs_add_dir(explorer_dir, "NEWFOLDER");
        if (nd >= 0) {
            fs_add_file(nd, "hello.mpp", "text+textbox=Hello World!");
            fs_add_file(nd, "hello.mex", "text+textbox=Hello World!");
            explorer_open_dir(nd);
            fs_save_to_disk();
        }
        return 1;
    }
    if (explorer_win.visible && point_in_rect(mx, my, explorer_win.x + 98, explorer_win.y + 24, 44, 10)) {
        int id = fs_child_by_index(explorer_dir, explorer_sel);
        if (id >= 0) {
            str_copy(fs[id].name, "RENAMED", MAX_NAME);
            fs_save_to_disk();
        }
        return 1;
    }

    if (explorer_win.visible) {
        int i;
        for (i = 0; i < 8; i++) {
            int id = fs_child_by_index(explorer_dir, i);
            int y = explorer_win.y + 42 + i * 12;
            if (id < 0) break;
            if (point_in_rect(mx, my, explorer_win.x + 8, y - 1, explorer_win.w - 16, 10)) {
                explorer_sel = i;
                if (fs[id].is_dir) {
                    explorer_open_dir(id);
                } else {
                    if (str_ends_with(fs[id].name, ".mpp")) {
                        str_copy(studio_code, fs[id].content, MAX_CODE);
                        studio_len = str_len(studio_code);
                        studio_win.visible = 1;
                    }
                    if (str_ends_with(fs[id].name, ".mex")) {
                        run_mex_text(fs[id].content);
                    }
                }
                return 1;
            }
        }
    }

    if (virus_win.visible && point_in_rect(mx, my, virus_win.x + 20, virus_win.y + 46, 90, 20)) {
        warn_win.visible = 1;
        return 1;
    }

    if (warn_win.visible && point_in_rect(mx, my, warn_win.x + 22, warn_win.y + 66, 58, 16)) {
        dance_mode = 1;
        warn_win.visible = 0;
        return 1;
    }

    if (warn_win.visible && point_in_rect(mx, my, warn_win.x + 126, warn_win.y + 66, 58, 16)) {
        warn_win.visible = 0;
        return 1;
    }

    return 0;
}

static void mouse_wait_write(void) {
    u32 t = 100000;
    while (t--) {
        if ((inb(0x64) & 2) == 0) return;
    }
}

static void mouse_wait_read(void) {
    u32 t = 100000;
    while (t--) {
        if (inb(0x64) & 1) return;
    }
}

static void mouse_write(u8 data) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, data);
}

static u8 mouse_read(void) {
    mouse_wait_read();
    return inb(0x60);
}

static void mouse_init(void) {
    u8 status;

    mouse_wait_write();
    outb(0x64, 0xA8);

    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    status = inb(0x60);
    status |= 2;

    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, status);

    mouse_write(0xF6);
    (void)mouse_read();

    mouse_write(0xF4);
    (void)mouse_read();
}

static void process_mouse_packet(int* moved, int* left_clicked, int* left_changed) {
    s32 dx = (s32)((signed char)mouse_packet[1]);
    s32 dy = (s32)((signed char)mouse_packet[2]);
    u8 new_left = (mouse_packet[0] & 0x01) ? 1 : 0;

    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x > (WIDTH - CURSOR_W)) mouse_x = WIDTH - CURSOR_W;
    if (mouse_y > (HEIGHT - CURSOR_H)) mouse_y = HEIGHT - CURSOR_H;

    if (mouse_x != prev_mouse_x || mouse_y != prev_mouse_y) {
        *moved = 1;
    }

    if (new_left != left_down) {
        *left_changed = 1;
        if (!left_down && new_left) {
            *left_clicked = 1;
        }
        left_down = new_left;
    }
}

static void poll_input(int* moved, int* left_clicked, int* ui_dirty) {
    while (inb(0x64) & 1) {
        u8 status = inb(0x64);

        if (status & 0x20) {
            u8 b = inb(0x60);
            if (mouse_cycle == 0 && (b & 0x08) == 0) {
                continue;
            }

            mouse_packet[mouse_cycle++] = b;
            if (mouse_cycle == 3) {
                int left_changed = 0;
                mouse_cycle = 0;
                process_mouse_packet(moved, left_clicked, &left_changed);
                if (left_changed && status_win.visible) {
                    *ui_dirty = 1;
                }
            }
        } else {
            u8 code = inb(0x60);
            if ((code & 0x80) == 0) {
                char calc_key;
                char ch;
                if (code != last_scancode) {
                    last_scancode = code;
                    if (status_win.visible) {
                        *ui_dirty = 1;
                    }
                }

                calc_key = scancode_to_calc_key(code);
                if (calc_key) {
                    calc_press(calc_key);
                    if (calc_win.visible) {
                        *ui_dirty = 1;
                    }
                }

                if (studio_win.visible) {
                    ch = scancode_to_ascii(code);
                    if (filedlg_win.visible) {
                        if (ch == '\n') {
                            filedlg_execute();
                            *ui_dirty = 1;
                        } else if (ch == '\b') {
                            if (filedlg_len > 0) {
                                filedlg_len--;
                                filedlg_name[filedlg_len] = 0;
                                *ui_dirty = 1;
                            }
                        } else if (ch) {
                            if (filedlg_len < 31) {
                                filedlg_name[filedlg_len++] = ch;
                                filedlg_name[filedlg_len] = 0;
                                *ui_dirty = 1;
                            }
                        }
                    } else {
                        if (ch == '\b') {
                            if (studio_len > 0) {
                                studio_len--;
                                studio_code[studio_len] = 0;
                                *ui_dirty = 1;
                            }
                        } else if (ch && ch != '\n') {
                            if (studio_len < MAX_CODE - 1) {
                                studio_code[studio_len++] = ch;
                                studio_code[studio_len] = 0;
                                *ui_dirty = 1;
                            }
                        }
                    }
                }
            }
        }
    }
}

void kmain(void) {
    int ui_dirty = 1;

    fs_init();
    studio_len = str_len(studio_code);
    filedlg_len = str_len(filedlg_name);
    draw_splash_to_bg();
    blit_full_from_bg();
    boot_beep();
    splash_wait_2s();

    calc_clear();
    mouse_init();

    draw_desktop_to_bg();
    blit_full_from_bg();
    draw_mouse_cursor(mouse_x, mouse_y);
    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    ui_dirty = 0;

    for (;;) {
        int moved = 0;
        int left_clicked = 0;
        s32 hit_x;
        s32 hit_y;
        s32 dance_dx = 0;
        s32 dance_dy = 0;

        poll_input(&moved, &left_clicked, &ui_dirty);
        hit_x = cursor_hit_x();
        hit_y = cursor_hit_y();

        if (dance_mode) {
            dance_tick++;
            dance_dx = (s32)((dance_tick % 7) - 3);
            dance_dy = (s32)(((dance_tick / 2) % 5) - 2);
            ui_dirty = 1;
        }

        if (!left_down) {
            drag_win = 0;
        }

        if (left_clicked) {
            if (start_drag_if_hit(hit_x, hit_y)) {
                ui_dirty = 1;
            } else if (handle_ui_click(hit_x, hit_y)) {
                ui_dirty = 1;
            }
        }

        if (left_down && drag_win) {
            s32 nx = hit_x - drag_off_x;
            s32 ny = hit_y - drag_off_y;
            if (drag_win->x != nx || drag_win->y != ny) {
                drag_win->x = nx;
                drag_win->y = ny;
                clamp_window(drag_win);
                ui_dirty = 1;
            }
        }

        if (ui_dirty) {
            restore_under_cursor(prev_mouse_x, prev_mouse_y);
            draw_desktop_to_bg();
            if (dance_mode) {
                blit_shift_from_bg(dance_dx, dance_dy);
            } else {
                blit_full_from_bg();
            }
            draw_mouse_cursor(mouse_x, mouse_y);
            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;
            ui_dirty = 0;
            moved = 0;
        }

        if (moved) {
            restore_under_cursor(prev_mouse_x, prev_mouse_y);
            draw_mouse_cursor(mouse_x, mouse_y);
            prev_mouse_x = mouse_x;
            prev_mouse_y = mouse_y;
        }

        io_wait();
    }
}
