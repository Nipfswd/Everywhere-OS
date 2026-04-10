#include <stdint.h>

/* --- CONFIGURATION --- */
#define MAX_FILES 10
#define MAX_FILENAME 32
#define MAX_CONTENT 512
#define MAX_SNAKE 100
volatile char* VIDEO_BUF = (volatile char*)0xb8000;

/* --- STORAGE --- */
typedef struct {
    char name[MAX_FILENAME];
    char content[MAX_CONTENT];
    int active;
} File;

File file_system[MAX_FILES];
int file_count = 0;
int cursor_pos = 0;
int shift_pressed = 0;
int high_score = 0;

/* --- HARDWARE I/O --- */
void outb(uint16_t port, uint8_t data) { __asm__("outb %1, %0" : : "dN" (port), "a" (data)); }
uint8_t inb(uint16_t port) { uint8_t res; __asm__("inb %1, %0" : "=a" (res) : "Nd" (port)); return res; }

void update_cursor() {
    uint16_t pos = cursor_pos / 2;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* --- SCREEN & STRING --- */
void print_char(char c, char attr) {
    if (c == '\n') cursor_pos = (cursor_pos / 160 + 1) * 160;
    else { VIDEO_BUF[cursor_pos++] = c; VIDEO_BUF[cursor_pos++] = attr; }
    update_cursor();
}

void print(const char* s) { for(int i=0; s[i]; i++) print_char(s[i], 0x07); }

void print_int(int n) {
    if (n == 0) { print_char('0', 0x07); return; }
    char buf[10]; int i = 0;
    while (n > 0) { buf[i++] = (n % 10) + '0'; n /= 10; }
    while (--i >= 0) print_char(buf[i], 0x07);
}

void clear_screen() {
    for(int i=0; i<4000; i+=2) { VIDEO_BUF[i]=' '; VIDEO_BUF[i+1]=0x07; }
    cursor_pos=0; update_cursor();
}

int strcmp(const char* s1, const char* s2) {
    while(*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(uint8_t*)s1 - *(uint8_t*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while(n--) { if(*s1 != *s2++) return 1; if(*s1++ == 0) break; }
    return 0;
}

/* --- KEYBOARD --- */
char get_char() {
    char lower[] = {0,0,'1','2','3','4','5','6','7','8','9','0','-','=',0,0,'q','w','e','r','t','y','u','i','o','p','[',']',0,0,'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v','b','n','m',',','.','/',0,'*',0,' '};
    char upper[] = {0,0,'!','@','#','$','%','^','&','*','(',')','_','+',0,0,'Q','W','E','R','T','Y','U','I','O','P','{','}',0,0,'A','S','D','F','G','H','J','K','L',':','\"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',0,'*',0,' '};
    while(1) {
        if(inb(0x64) & 0x01) {
            uint8_t s = inb(0x60);
            if(s == 0x2A || s == 0x36) { shift_pressed = 1; continue; }
            if(s == 0xAA || s == 0xB6) { shift_pressed = 0; continue; }
            if(s & 0x80) continue;
            if(s == 0x0E) return 0x08; // Backspace
            if(s == 0x1C) return '\n';
            return shift_pressed ? upper[s] : lower[s];
        }
    }
}

void get_input(char* buf) {
    int i = 0;
    while(1) {
        char c = get_char();
        if(c == '\n') { buf[i]='\0'; print_char('\n',0x07); break; }
        else if(c == 0x08 && i > 0) { i--; cursor_pos-=2; VIDEO_BUF[cursor_pos]=' '; update_cursor(); }
        else if(c != 0x08) { buf[i++] = c; print_char(c, 0x07); }
    }
}

/* --- SNAKE GAME --- */
void snake_game() {
    int sx[MAX_SNAKE], sy[MAX_SNAKE];
    int length = 1, fx = 20, fy = 10, dx = 1, dy = 0, score = 0;
    sx[0] = 40; sy[0] = 12;
    clear_screen();
    while(1) {
        int old = cursor_pos; cursor_pos = 0;
        print("Score: "); print_int(score); print(" | WASD to Move, Q to Quit");
        cursor_pos = old; update_cursor();

        VIDEO_BUF[(fy * 80 + fx) * 2] = '@'; VIDEO_BUF[(fy * 80 + fx) * 2 + 1] = 0x04;
        for (int i=0; i<length; i++) {
            VIDEO_BUF[(sy[i] * 80 + sx[i]) * 2] = (i==0)?'O':'*';
            VIDEO_BUF[(sy[i] * 80 + sx[i]) * 2 + 1] = 0x02;
        }

        for (volatile int d = 0; d < 12000000; d++); 

        if (inb(0x64) & 0x01) {
            unsigned char s = inb(0x60);
            if (s == 0x11 && dy != 1) { dx = 0; dy = -1; }
            if (s == 0x1E && dx != 1) { dx = -1; dy = 0; }
            if (s == 0x1F && dy != -1) { dx = 0; dy = 1; }
            if (s == 0x20 && dx != -1) { dx = 1; dy = 0; }
            if (s == 0x10) break;
        }

        VIDEO_BUF[(sy[length-1] * 80 + sx[length-1]) * 2] = ' ';
        for (int i = length - 1; i > 0; i--) { sx[i] = sx[i-1]; sy[i] = sy[i-1]; }
        sx[0] += dx; sy[0] += dy;

        if (sx[0] < 0 || sx[0] >= 80 || sy[0] < 1 || sy[0] >= 25) break; 
        if (sx[0] == fx && sy[0] == fy) {
            score++; if (length < MAX_SNAKE) length++;
            fx = (fx * 3 + 7) % 70 + 5; fy = (fy * 7 + 3) % 20 + 2;
        }
    }
    clear_screen(); print("Game Over! Score: "); print_int(score); print("\nAny key..."); get_char();
}

/* --- COMMAND SYSTEM & BOX LANGUAGE --- */
void process_command(char* cmd) {
    if (strcmp(cmd, "clear") == 0) clear_screen();
    else if (strncmp(cmd, "show ", 5) == 0) { print(cmd + 5); print("\n"); }
    else if (strcmp(cmd, "snake") == 0) snake_game();
    else if (strcmp(cmd, "new note") == 0) {
        if(file_count >= MAX_FILES) { print("Storage Full!\n"); return; }
        print("Filename: "); char name[32]; get_input(name);
        print("note (` to save):\n");
        int i=0; char c;
        while((c=get_char()) != '`' && i < MAX_CONTENT-1) {
            if(c==0x08 && i>0) { i--; cursor_pos-=2; VIDEO_BUF[cursor_pos]=' '; update_cursor(); }
            else if(c!=0x08) { file_system[file_count].content[i++] = c; print_char(c, 0x02); }
        }
        file_system[file_count].content[i] = '\0';
        for(int x=0; name[x]; x++) file_system[file_count].name[x] = name[x];
        file_system[file_count].active = 1; file_count++;
        print("\nSaved!\n");
    } 
    else if (strncmp(cmd, "box ", 4) == 0) {
        char* target = cmd + 4;
        for(int i=0; i<file_count; i++) {
            if(file_system[i].active && strcmp(file_system[i].name, target) == 0) {
                // BOX Interpreter: Execute each line as a command
                char line_buf[64]; int lb=0;
                for(int j=0; file_system[i].content[j] != '\0'; j++) {
                    if(file_system[i].content[j] == '\n') {
                        line_buf[lb] = '\0'; process_command(line_buf); lb = 0;
                    } else { line_buf[lb++] = file_system[i].content[j]; }
                }
                if(lb > 0) { line_buf[lb] = '\0'; process_command(line_buf); }
                return;
            }
        }
        print("File not found.\n");
    }
}

void kernelMain(void) {
    clear_screen();
    print("Everything OS v4.0\n");
    char buffer[128];
    while(1) {
        print("User: ");
        get_input(buffer);
        process_command(buffer);
    }
}

