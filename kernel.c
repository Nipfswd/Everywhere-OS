/*++

    Everywhere OS -  GUI Kernel (Single C File For Now, Until It Can Not Be One)
    ------------------------------------------------------
    - VGA Mode 13h (320x200x256)
    - PS/2 Mouse
    - Desktop + Windows 
    - Shell Window
    - Notes Window
    - Snake Window
    - Taskbar + Icons (lightweight)
    - Boot message:
      "Why Go Anywhere When You Are Everywhere? Everywhere OS"

--*/

#include <stdint.h>

/* ========== Basic I/O ========== */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ========== Reboot ========== */

void RebootSystem(void) {
    while (inb(0x64) & 0x02) { }
    outb(0x64, 0xFE);
    for (;;) { }
}

/* ========== VGA Mode 13h ========== */

#define SCR_W 320
#define SCR_H 200

uint8_t* FB = (uint8_t*)0xA0000;

void SetMode13h(void) {
    __asm__ __volatile__(
        "mov $0x13, %%ax\n"
        "int $0x10\n"
        :
        :
        : "ax"
    );
}

void PutPixel(int x, int y, uint8_t c) {
    if (x < 0 || x >= SCR_W || y < 0 || y >= SCR_H) return;
    FB[y * SCR_W + x] = c;
}

void FillRect(int x, int y, int w, int h, uint8_t c) {
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < 0 || py >= SCR_H) continue;
        for (int xx = 0; xx < w; xx++) {
            int px = x + xx;
            if (px < 0 || px >= SCR_W) continue;
            FB[py * SCR_W + px] = c;
        }
    }
}

/* ========== Tiny 8x8 Font (simple blocky) ========== */

uint8_t Font8x8[128][8];

void InitFont(void) {
    for (int c = 0; c < 128; c++) {
        for (int r = 0; r < 8; r++) {
            Font8x8[c][r] = 0x3C; /* 00111100 */
        }
    }
}

void DrawChar(int x, int y, char ch, uint8_t color) {
    if ((unsigned char)ch > 127) return;
    uint8_t* g = Font8x8[(int)ch];
    for (int r = 0; r < 8; r++) {
        uint8_t line = g[r];
        for (int c = 0; c < 8; c++) {
            if (line & (1 << (7 - c))) {
                PutPixel(x + c, y + r, color);
            }
        }
    }
}

void DrawString(int x, int y, const char* s, uint8_t color) {
    int cx = x;
    while (*s) {
        if (*s == '\n') {
            y += 8;
            cx = x;
        } else {
            DrawChar(cx, y, *s, color);
            cx += 8;
        }
        s++;
    }
}

/* ========== Desktop + Windows ========== */

typedef struct {
    int x, y, w, h;
    int vx, vy;       /* velocity for Physics */
    const char* title;
    int visible;
    int minimized;
    int dragging;
    int drag_off_x;
    int drag_off_y;
} WINDOW;

WINDOW ShellWin  = { 10, 10, 300, 70, 0, 0, "Shell", 1, 0, 0, 0, 0 };
WINDOW NotesWin  = { 10, 85, 300, 70, 0, 0, "Notes", 1, 0, 0, 0, 0 };
WINDOW SnakeWin  = { 60, 40, 200, 120, 0, 0, "Snake", 0, 0, 0, 0, 0 };

void DrawDesktop(void) {
    /* Retro Chaos: deep blue background, taskbar */
    FillRect(0, 0, SCR_W, SCR_H, 0x01);
    FillRect(0, SCR_H - 12, SCR_W, 12, 0x08);
    DrawString(4, SCR_H - 10, "Everywhere OS", 0x0F);
}

/* Title bar: magenta, border teal, buttons yellow */
void DrawWindowFrame(WINDOW* w) {
    if (!w->visible || w->minimized) return;

    /* Border */
    FillRect(w->x - 1, w->y - 1, w->w + 2, w->h + 2, 0x03);
    /* Body */
    FillRect(w->x, w->y, w->w, w->h, 0x00);
    /* Title bar */
    FillRect(w->x, w->y, w->w, 10, 0x0D);
    DrawString(w->x + 3, w->y + 1, w->title, 0x0F);

    /* Close button [X] */
    FillRect(w->x + w->w - 10, w->y + 1, 8, 8, 0x0E);
    DrawChar(w->x + w->w - 9, w->y + 1, 'X', 0x00);

    /* Minimize button [_] */
    FillRect(w->x + w->w - 20, w->y + 1, 8, 8, 0x0E);
    DrawChar(w->x + w->w - 19, w->y + 1, '_', 0x00);
}

/* Simple hit test for title bar and buttons */
int PointInRect(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x < rx + rw && y >= ry && y < ry + rh);
}

/* ========== Mouse ========== */

int mouse_x = SCR_W / 2;
int mouse_y = SCR_H / 2;
int mouse_buttons = 0;
int mouse_prev_buttons = 0;

void MouseWait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if (inb(0x64) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(0x64) & 2)) return;
        }
    }
}

void MouseWrite(uint8_t data) {
    MouseWait(1);
    outb(0x64, 0xD4);
    MouseWait(1);
    outb(0x60, data);
}

uint8_t MouseRead(void) {
    MouseWait(0);
    return inb(0x60);
}

void InitMouse(void) {
    uint8_t status;

    outb(0x64, 0xA8);
    MouseWait(1);
    outb(0x64, 0x20);
    MouseWait(0);
    status = inb(0x60) | 2;
    MouseWait(1);
    outb(0x64, 0x60);
    MouseWait(1);
    outb(0x60, status);

    MouseWrite(0xF6);
    MouseRead();
    MouseWrite(0xF4);
    MouseRead();
}

void UpdateMouse(void) {
    if (!(inb(0x64) & 1)) return;

    static uint8_t packet[3];
    static int idx = 0;

    packet[idx++] = inb(0x60);
    if (idx < 3) return;
    idx = 0;

    int dx = (int8_t)packet[1];
    int dy = (int8_t)packet[2];

    mouse_prev_buttons = mouse_buttons;
    mouse_buttons = packet[0] & 0x07;

    mouse_x += dx;
    mouse_y -= dy;

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x >= SCR_W) mouse_x = SCR_W - 1;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y >= SCR_H) mouse_y = SCR_H - 1;
}

void DrawMouseCursor(void) {
    /* Bright red triangle */
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j <= i; j++) {
            PutPixel(mouse_x + j, mouse_y + i, 0x04);
        }
    }
}

/* ========== Keyboard ========== */

int shift_pressed = 0;

char GetKeyChar(void) {
    if (!(inb(0x64) & 1)) return 0;

    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; return 0; }

    if (sc & 0x80) return 0;

    if (sc == 0x1C) return '\n';
    if (sc == 0x0E) return 0x08;
    if (sc == 0x01) return 27;

    static char Lower[] = {
        0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,
        'q','w','e','r','t','y','u','i','o','p','[',']',0,0,
        'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
        'z','x','c','v','b','n','m',',','.','/',0,'*',0,' '
    };

    static char Upper[] = {
        0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,
        'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,
        'A','S','D','F','G','H','J','K','L',':','\"','~',0,'|',
        'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '
    };

    if (sc < sizeof(Lower)) {
        return shift_pressed ? Upper[sc] : Lower[sc];
    }

    return 0;
}

/* ========== Shell ========== */

char shell_input[64];
int  shell_len = 0;

void ShellClear(void) {
    FillRect(ShellWin.x + 2, ShellWin.y + 12, ShellWin.w - 4, ShellWin.h - 14, 0x00);
}

void ShellDraw(void) {
    if (!ShellWin.visible || ShellWin.minimized) return;
    DrawString(ShellWin.x + 4, ShellWin.y + 14, shell_input, 0x0F);
}

int StrEq(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (*a == 0 && *b == 0);
}

void ShellExec(void) {
    if (shell_len == 0) return;

    if (StrEq(shell_input, "clear")) {
        ShellClear();
    } else if (StrEq(shell_input, "credits")) {
        DrawString(ShellWin.x + 4, ShellWin.y + 24,
                   "Clay Sanders, Noah Juopperi\n", 0x0F);
    } else if (StrEq(shell_input, "snake")) {
        SnakeWin.visible = 1;
        SnakeWin.minimized = 0;
    } else if (StrEq(shell_input, "notes")) {
        NotesWin.visible = 1;
        NotesWin.minimized = 0;
    }

    shell_len = 0;
    shell_input[0] = 0;
}

/* ========== Notes ========== */

char notes_buf[256];
int  notes_len = 0;

void NotesDraw(void) {
    if (!NotesWin.visible || NotesWin.minimized) return;
    DrawString(NotesWin.x + 4, NotesWin.y + 14, notes_buf, 0x0F);
}

/* ========== Snake ========== */

#define SNAKE_MAX 100
int snake_x[SNAKE_MAX];
int snake_y[SNAKE_MAX];
int snake_len = 5;
int snake_dx = 1;
int snake_dy = 0;
int food_x = 20;
int food_y = 10;

void SnakeInit(void) {
    for (int i = 0; i < snake_len; i++) {
        snake_x[i] = 10 - i;
        snake_y[i] = 10;
    }
}

void SnakeStep(void) {
    if (!SnakeWin.visible || SnakeWin.minimized) return;

    for (int i = snake_len - 1; i > 0; i--) {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] += snake_dx;
    snake_y[0] += snake_dy;

    if (snake_x[0] < 0) snake_x[0] = 0;
    if (snake_y[0] < 0) snake_y[0] = 0;
    if (snake_x[0] > SnakeWin.w - 10) snake_x[0] = SnakeWin.w - 10;
    if (snake_y[0] > SnakeWin.h - 20) snake_y[0] = SnakeWin.h - 20;

    if (snake_x[0] == food_x && snake_y[0] == food_y) {
        if (snake_len < SNAKE_MAX) snake_len++;
        food_x = (food_x * 7 + 13) % (SnakeWin.w - 10);
        food_y = (food_y * 5 + 11) % (SnakeWin.h - 20);
    }
}

void SnakeDraw(void) {
    if (!SnakeWin.visible || SnakeWin.minimized) return;

    FillRect(SnakeWin.x + 2, SnakeWin.y + 12, SnakeWin.w - 4, SnakeWin.h - 14, 0x00);

    for (int i = 0; i < snake_len; i++) {
        PutPixel(SnakeWin.x + 5 + snake_x[i],
                 SnakeWin.y + 15 + snake_y[i], 0x0A);
    }

    PutPixel(SnakeWin.x + 5 + food_x,
             SnakeWin.y + 15 + food_y, 0x04);
}

/* ========== Window Drag + Clay Physics ========== */

void UpdateWindowPhysics(WINDOW* w) {
    if (!w->visible || w->minimized) return;

    w->x += w->vx;
    w->y += w->vy;

    /* friction */
    w->vx = (w->vx * 9) / 10;
    w->vy = (w->vy * 9) / 10;

    /* bounce on edges */
    if (w->x < 0) { w->x = 0; w->vx = -w->vx / 2; }
    if (w->y < 0) { w->y = 0; w->vy = -w->vy / 2; }
    if (w->x + w->w > SCR_W) { w->x = SCR_W - w->w; w->vx = -w->vx / 2; }
    if (w->y + w->h > SCR_H - 12) { w->y = SCR_H - 12 - w->h; w->vy = -w->vy / 2; }
}

void HandleWindowMouse(WINDOW* w) {
    if (!w->visible) return;

    int left_down  = mouse_buttons & 1;
    int left_prev  = mouse_prev_buttons & 1;

    /* Click on title bar to drag */
    if (!w->dragging && left_down && !left_prev &&
        PointInRect(mouse_x, mouse_y, w->x, w->y, w->w, 10)) {
        w->dragging = 1;
        w->drag_off_x = mouse_x - w->x;
        w->drag_off_y = mouse_y - w->y;
        w->vx = 0;
        w->vy = 0;
    }

    if (w->dragging && left_down) {
        int new_x = mouse_x - w->drag_off_x;
        int new_y = mouse_y - w->drag_off_y;
        w->vx = new_x - w->x;
        w->vy = new_y - w->y;
        w->x = new_x;
        w->y = new_y;
    }

    if (w->dragging && !left_down && left_prev) {
        w->dragging = 0;
    }

    /* Close button */
    if (left_down && !left_prev &&
        PointInRect(mouse_x, mouse_y, w->x + w->w - 10, w->y + 1, 8, 8)) {
        w->visible = 0;
    }

    /* Minimize button */
    if (left_down && !left_prev &&
        PointInRect(mouse_x, mouse_y, w->x + w->w - 20, w->y + 1, 8, 8)) {
        w->minimized = 1;
    }
}

/* ========== Taskbar (simple) ========== */

void DrawTaskbar(void) {
    FillRect(0, SCR_H - 12, SCR_W, 12, 0x08);
    DrawString(4, SCR_H - 10, "Everywhere OS", 0x0F);

    int x = 120;
    if (ShellWin.visible && ShellWin.minimized) {
        FillRect(x, SCR_H - 11, 40, 10, 0x03);
        DrawString(x + 2, SCR_H - 10, "Shell", 0x0F);
        x += 44;
    }
    if (NotesWin.visible && NotesWin.minimized) {
        FillRect(x, SCR_H - 11, 40, 10, 0x03);
        DrawString(x + 2, SCR_H - 10, "Notes", 0x0F);
        x += 44;
    }
    if (SnakeWin.visible && SnakeWin.minimized) {
        FillRect(x, SCR_H - 11, 40, 10, 0x03);
        DrawString(x + 2, SCR_H - 10, "Snake", 0x0F);
        x += 44;
    }
}

/* ========== Boot Screen ========== */

void DrawBootScreen(void) {
    FillRect(0, 0, SCR_W, SCR_H, 0x00);
    DrawString(10, 80,
        "Why Go Anywhere When You Are Everywhere?\nEverywhere OS",
        0x0F);
}

/* ========== Main ========== */

void kernelMain(void) {
    SetMode13h();
    InitFont();
    InitMouse();
    SnakeInit();

    DrawBootScreen();
    for (volatile int d = 0; d < 2000000; d++) { __asm__ __volatile__("nop"); }

    while (1) {
        char ch = GetKeyChar();
        if (ch == 27) {
            RebootSystem();
        }

        /* Shell input */
        if (ch) {
            if (ch == '\n') {
                ShellExec();
            } else if (ch == 0x08) {
                if (shell_len > 0) shell_input[--shell_len] = 0;
            } else if (shell_len < (int)sizeof(shell_input) - 1) {
                shell_input[shell_len++] = ch;
                shell_input[shell_len] = 0;
            }
        }

        UpdateMouse();
        SnakeStep();

        HandleWindowMouse(&ShellWin);
        HandleWindowMouse(&NotesWin);
        HandleWindowMouse(&SnakeWin);

        UpdateWindowPhysics(&ShellWin);
        UpdateWindowPhysics(&NotesWin);
        UpdateWindowPhysics(&SnakeWin);

        DrawDesktop();
        DrawWindowFrame(&ShellWin);
        DrawWindowFrame(&NotesWin);
        DrawWindowFrame(&SnakeWin);

        ShellDraw();
        NotesDraw();
        SnakeDraw();

        DrawTaskbar();
        DrawMouseCursor();

        for (volatile int d = 0; d < 30000; d++) { __asm__ __volatile__("nop"); }
    }
}