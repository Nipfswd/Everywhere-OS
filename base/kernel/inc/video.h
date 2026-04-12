/*++

Module Name:

    video.h

Abstract:

    Public interface to the video subsystem. Supports both a legacy
    VGA text fallback and a VESA linear framebuffer pixel mode.

Author:

    Noah Juopperi <nipfswd@gmail.com>

--*/

#pragma once

#include "types.h"
#include <stdint.h>

/* RGB colour helpers */
#define RGB(r,g,b)  (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

#define COLOR_BLACK      RGB(0,   0,   0)
#define COLOR_WHITE      RGB(255, 255, 255)
#define COLOR_GRAY       RGB(192, 192, 192)
#define COLOR_DARK_GRAY  RGB(64,  64,  64)
#define COLOR_GREEN      RGB(0,   200, 80)
#define COLOR_RED        RGB(220, 50,  50)
#define COLOR_BLUE       RGB(30,  100, 220)
#define COLOR_CYAN       RGB(0,   200, 220)
#define COLOR_YELLOW     RGB(240, 220, 0)

/* Console text colours (mapped from VGA attribute nibbles) */
#define ATTR_NORMAL 0x07
#define ATTR_GREEN  0x0A
#define ATTR_RED    0x0C
#define ATTR_CYAN   0x0B
#define ATTR_YELLOW 0x0E
#define ATTR_WHITE  0x0F

/*
 * VideoInitialize
 *   Must be called once with the multiboot framebuffer fields before
 *   any other video routine.  Falls back to VGA text mode if the
 *   framebuffer address is zero.
 */
VOID VideoInitialize(uint64_t Address, uint32_t Width, uint32_t Height,
                     uint32_t Pitch,   uint8_t  Bpp);

/* Low-level pixel primitives */
VOID VideoSetPixel  (uint32_t X, uint32_t Y, uint32_t Color);
VOID VideoFillRect  (uint32_t X, uint32_t Y, uint32_t W, uint32_t H, uint32_t Color);
VOID VideoDrawRect  (uint32_t X, uint32_t Y, uint32_t W, uint32_t H, uint32_t Color);
VOID VideoDrawLine  (int X0, int Y0, int X1, int Y1, uint32_t Color);

/* Glyph / text rendering */
VOID VideoDrawChar  (uint32_t X, uint32_t Y, char C, uint32_t Fg, uint32_t Bg);
VOID VideoDrawString(uint32_t X, uint32_t Y, const char *S, uint32_t Fg, uint32_t Bg);

/* Scrolling console (replaces old Print/PrintChar/ClearScreen) */
VOID PrintChar  (char Character, char Attribute);
VOID Print      (const char *String);
VOID PrintInt   (int Number);
VOID ClearScreen(VOID);

/*
 * Direct grid-cell access — Col/Row are in character units (0-based).
 * VideoSetCell draws a character at that grid position without moving
 * the console cursor.  VideoSetConsoleCursor repositions the console
 * cursor so the next PrintChar lands at (Col, Row).
 */
VOID VideoSetCell         (uint32_t Col, uint32_t Row, char C, char Attribute);
VOID VideoSetConsoleCursor(uint32_t Col, uint32_t Row);

/* Screen dimensions (set after VideoInitialize) */
extern uint32_t g_ScreenWidth;
extern uint32_t g_ScreenHeight;