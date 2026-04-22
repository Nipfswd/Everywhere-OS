/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    taskbar.c

Abstract:

    Taskbar drawing and click handling for minimized window restoration.

Author:

    Noah Juopperi <nipfswd@gmail.com>
    Clay Sanders (made the first version of the kernel) <claylikepython@yahoo.com>

Environment:

    Userspace

--*/

#include "explorer.h"

/*++

Routine Description:

    Handles mouse clicks on taskbar items to restore minimized windows.

Arguments:

    None.

Return Value:

    None.

--*/

void HandleTaskbarClick(void) {
    int left_down = mouse_buttons & 1;
    int left_prev = mouse_prev_buttons & 1;
    if (!(left_down && !left_prev)) return;
    if (!PointInRect(mouse_x, mouse_y, 0, SCR_H - 12, SCR_W, 12)) return;

    int x = 105;
    int btn_w = 70;

    if (ShellWin.visible && PointInRect(mouse_x, mouse_y, x, SCR_H - 11, btn_w, 10)) {
        if (ShellWin.minimized)
            ShellWin.minimized = 0;
        else
            ShellWin.minimized = 1;
        active_window = 0;
        return;
    }
    x += btn_w + 2;

    if (NotesWin.visible && PointInRect(mouse_x, mouse_y, x, SCR_H - 11, btn_w, 10)) {
        if (NotesWin.minimized)
            NotesWin.minimized = 0;
        else
            NotesWin.minimized = 1;
        active_window = 1;
        return;
    }
    x += btn_w + 2;

    if (SnakeWin.visible && PointInRect(mouse_x, mouse_y, x, SCR_H - 11, btn_w, 10)) {
        if (SnakeWin.minimized)
            SnakeWin.minimized = 0;
        else
            SnakeWin.minimized = 1;
        active_window = 2;
        return;
    }
    x += btn_w + 2;

    if (FilesWin.visible && PointInRect(mouse_x, mouse_y, x, SCR_H - 11, btn_w, 10)) {
        if (FilesWin.minimized)
            FilesWin.minimized = 0;
        else
            FilesWin.minimized = 1;
        active_window = 3;
        return;
    }
}

/*++

Routine Description:

    Draws the taskbar with all visible windows as buttons and highlights the active window.

Arguments:

    None.

Return Value:

    None.

--*/

void DrawTaskbar(void) {
    int x = 105;
    int btn_w = 70;
    int btn_h = 10;

    if (ShellWin.visible) {
        uint8_t bg = (active_window == 0) ? 0x0F : 0x03;
        uint8_t fg = (active_window == 0) ? 0x00 : 0x0F;
        FillRect(x, SCR_H - 11, btn_w, btn_h, bg);
        FillRect(x + 1, SCR_H - 10, btn_w - 2, btn_h - 2, bg);
        if (ShellWin.minimized)
            DrawString(x + 3, SCR_H - 10, "[Shell]", fg);
        else
            DrawString(x + 5, SCR_H - 10, "Shell", fg);
        x += btn_w + 2;
    }

    if (NotesWin.visible) {
        uint8_t bg = (active_window == 1) ? 0x0F : 0x03;
        uint8_t fg = (active_window == 1) ? 0x00 : 0x0F;
        FillRect(x, SCR_H - 11, btn_w, btn_h, bg);
        FillRect(x + 1, SCR_H - 10, btn_w - 2, btn_h - 2, bg);
        if (NotesWin.minimized)
            DrawString(x + 3, SCR_H - 10, "[Notes]", fg);
        else
            DrawString(x + 5, SCR_H - 10, "Notes", fg);
        x += btn_w + 2;
    }

    if (SnakeWin.visible) {
        uint8_t bg = (active_window == 2) ? 0x0F : 0x03;
        uint8_t fg = (active_window == 2) ? 0x00 : 0x0F;
        FillRect(x, SCR_H - 11, btn_w, btn_h, bg);
        FillRect(x + 1, SCR_H - 10, btn_w - 2, btn_h - 2, bg);
        if (SnakeWin.minimized)
            DrawString(x + 3, SCR_H - 10, "[Snake]", fg);
        else
            DrawString(x + 5, SCR_H - 10, "Snake", fg);
        x += btn_w + 2;
    }

    if (FilesWin.visible) {
        uint8_t bg = (active_window == 3) ? 0x0F : 0x03;
        uint8_t fg = (active_window == 3) ? 0x00 : 0x0F;
        FillRect(x, SCR_H - 11, btn_w, btn_h, bg);
        FillRect(x + 1, SCR_H - 10, btn_w - 2, btn_h - 2, bg);
        if (FilesWin.minimized)
            DrawString(x + 3, SCR_H - 10, "[Files]", fg);
        else
            DrawString(x + 5, SCR_H - 10, "Files", fg);
        x += btn_w + 2;
    }
}
