/*++

Copyright (c) 2026  The EverywhereOS Authors

Module Name:

    files.c

Abstract:

    Files window: displays the EVRYFS directory listing. Each visible
    row shows a filename and its size in bytes.

Author:

    Noah Juopperi <nipfswd@gmail.com>

Environment:

    Userspace

--*/

#include "explorer.h"
#include "evryfs.h"

/* ---- Double-click state ----------------------------------------------- */

#define FILES_DBLCLICK_TICKS 20

static uint32_t files_last_click_tick = 0;
static int      files_last_click_row  = -1;

/* ---- Internal helpers -------------------------------------------------- */

/*++

Routine Description:

    Converts an unsigned 32-bit integer to a decimal ASCII string.

Arguments:

    n   - Value to convert.
    buf - Destination buffer (must hold at least 11 bytes).

Return Value:

    None.

--*/
static void FilesUintToStr(uint32_t n, char* buf)
{
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    char  tmp[12];
    int   i = 0;
    while (n > 0) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    int j;
    for (j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[j] = 0;
}

/*++

Routine Description:

    Copies src into dst for at most n-1 characters, always null-terminating.

--*/
static void FilesStrCpy(char* dst, const char* src, int n)
{
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/*++

Routine Description:

    Returns the length of a null-terminated string.

--*/
static int FilesStrLen(const char* s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* ---- Public routines --------------------------------------------------- */

/*++

Routine Description:

    Draws the EVRYFS directory listing inside the Files window. Each entry
    is rendered as "filename   <N> B" on its own row. If the volume has no
    files, "Empty" is shown. If no disk is present, "No disk" is shown.

Arguments:

    None.

Return Value:

    None.

--*/
void FilesDraw(void)
{
    if (!FilesWin.visible || FilesWin.minimized) return;

    char     names[EVRYFS_MAX_FILES][EVRYFS_NAME_LEN];
    uint32_t sizes[EVRYFS_MAX_FILES];
    int      count = 0;

    EvryFsList(names, sizes, &count);

    int y = FilesWin.y + 14;

    if (count == 0) {
        DrawString(FilesWin.x + 4, y, "Empty", 0x07);
        return;
    }

    for (int i = 0; i < count; i++) {
        if (y >= FilesWin.y + FilesWin.h - 2) break;

        /* Build row: "name   <size> B" */
        char row[48];
        char sizestr[12];

        FilesStrCpy(row, names[i], 32);

        /* Pad to column 20 */
        int len = FilesStrLen(row);
        while (len < 20 && len < 44) { row[len++] = ' '; }
        row[len] = 0;

        FilesUintToStr(sizes[i], sizestr);

        /* Append size + " B" */
        int slen = FilesStrLen(sizestr);
        for (int j = 0; j < slen && len < 44; j++) row[len++] = sizestr[j];
        if (len < 45) { row[len++] = ' '; }
        if (len < 46) { row[len++] = 'B'; }
        row[len] = 0;

        DrawString(FilesWin.x + 4, y, row, 0x0F);
        y += 10;
    }
}

/*++

Routine Description:

    Returns non-zero if the null-terminated string s ends with suffix.

--*/
static int FilesStrEndsWith(const char* s, const char* suffix)
{
    int slen    = FilesStrLen(s);
    int suflen  = FilesStrLen(suffix);
    if (suflen > slen) return 0;
    for (int i = 0; i < suflen; i++) {
        if (s[slen - suflen + i] != suffix[i]) return 0;
    }
    return 1;
}

/*++

Routine Description:

    Handles mouse clicks inside the Files window. A double-click on a
    .txt file loads its contents into the Notes window and brings the
    Notes window to the foreground.

Arguments:

    None.

Return Value:

    None.

--*/
void FilesHandleMouse(void)
{
    if (!FilesWin.visible || FilesWin.minimized) return;

    int left_down = mouse_buttons & 1;
    int left_prev = mouse_prev_buttons & 1;

    if (!(left_down && !left_prev)) return;

    int list_x = FilesWin.x + 4;
    int list_y = FilesWin.y + 14;
    int list_w = FilesWin.w - 8;
    int list_h = FilesWin.h - 16;

    if (!PointInRect(mouse_x, mouse_y, list_x, list_y, list_w, list_h)) {
        files_last_click_row = -1;
        return;
    }

    int row = (mouse_y - list_y) / 10;

    char     names[EVRYFS_MAX_FILES][EVRYFS_NAME_LEN];
    uint32_t sizes[EVRYFS_MAX_FILES];
    int      count = 0;
    EvryFsList(names, sizes, &count);

    if (row < 0 || row >= count) {
        files_last_click_row = -1;
        return;
    }

    if (row == files_last_click_row &&
        (ke_tick - files_last_click_tick) <= FILES_DBLCLICK_TICKS) {
        /* Double-click confirmed */
        if (FilesStrEndsWith(names[row], ".txt")) {
            int n = EvryFsReadFile(names[row], (uint8_t*)notes_buf, NOTES_BUF_SIZE - 1);
            if (n < 0) n = 0;
            notes_buf[n] = 0;
            notes_len = n;
            NotesWin.visible   = 1;
            NotesWin.minimized = 0;
            active_window = 1;
        }
        files_last_click_row = -1;
    } else {
        files_last_click_tick = ke_tick;
        files_last_click_row  = row;
    }
}
