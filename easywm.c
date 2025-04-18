#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#include "config.h"

typedef struct {
    Window windows[MAX_WINDOWS];
    int count;
    Bool is_floating[MAX_WINDOWS]; // Track floating state per window
} Desktop;

typedef enum { LAYOUT_TILE, LAYOUT_FLOAT } Layout;

// Global state
Display *display;
Window root;
Desktop desktops[DESKTOPS];
int current_desktop = 0;
Layout current_layout = LAYOUT_TILE;
int master_width_ratio = 60;
int focused_window_idx = 0;
float cpu_usage = 0;
unsigned long bar_color, bar_text_color, focused_color, normal_color;

// Function prototypes
void switch_desktop(int);
void spawn(const char*);
void draw_status_bar(void);
void tile_windows(void);
void manage_window(Window);
void kill_focused_window(void);
void move_window_to_desktop(int);
void update_cpu_usage(void);
void toggle_floating_window(void);

// Helper functions
unsigned long hex_to_xcolor(const char *hex) {
    int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);
    return (r << 16) | (g << 8) | b;
}

void spawn_terminal() { spawn(TERMINAL); }
void spawn_dmenu() { spawn(DMENU_CMD); }

void spawn(const char *cmd) {
    if (fork() == 0) {
        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        exit(EXIT_SUCCESS);
    }
}

void switch_desktop(int new_desktop) {
    if (new_desktop < 0 || new_desktop >= DESKTOPS) return;
    
    // Unmap old desktop
    for (int i = 0; i < desktops[current_desktop].count; i++)
        XUnmapWindow(display, desktops[current_desktop].windows[i]);
    
    // Map new desktop
    current_desktop = new_desktop;
    for (int i = 0; i < desktops[current_desktop].count; i++)
        XMapWindow(display, desktops[current_desktop].windows[i]);
    
    XFlush(display);
    draw_status_bar();
}

void update_cpu_usage() {
    static unsigned long long last_total = 0, last_idle = 0;
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    
    char line[256];
    if (fgets(line, sizeof(line), fp)) {
        unsigned long long user, nice, system, idle, iowait;
        sscanf(line + 5, "%llu %llu %llu %llu %llu", &user, &nice, &system, &idle, &iowait);
        unsigned long long total = user + nice + system + idle + iowait;
        unsigned long long total_diff = total - last_total;
        unsigned long long idle_diff = idle - last_idle;
        
        if (total_diff > 0)
            cpu_usage = 100.0 * (1.0 - (double)idle_diff / total_diff);
        
        last_total = total;
        last_idle = idle;
    }
    fclose(fp);
}

void draw_status_bar() {
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    
    // Clear bar
    XSetForeground(display, DefaultGC(display, 0), bar_color);
    XFillRectangle(display, root, DefaultGC(display, 0), 0, 0, screen_width, BAR_HEIGHT);

    // System info
    struct sysinfo info;
    sysinfo(&info);
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    
    char status[256];
    strftime(status, sizeof(status), "%a %b %d %H:%M:%S | ", tm);
    
    char sysinfo[128];
    snprintf(sysinfo, sizeof(sysinfo), 
        "CPU: %.1f%% | RAM: %.1f%% | WS:%d/%d %s",
        cpu_usage,
        (1.0 - (double)info.freeram/info.totalram) * 100,
        current_desktop + 1,
        DESKTOPS,
        current_layout == LAYOUT_TILE ? "[]=" : "><>"
    );
    strcat(status, sysinfo);

    // Draw text (centered vertically)
    XftDraw *draw = XftDrawCreate(display, root,
        DefaultVisual(display, 0), DefaultColormap(display, 0));
    
    XftColor color;
    XftColorAllocName(display, DefaultVisual(display, 0),
        DefaultColormap(display, 0), BAR_TEXT_COLOR, &color);
    
    XftFont *font = XftFontOpenName(display, 0, FONT);
    int text_y = (BAR_HEIGHT - font->ascent + font->descent) / 2;
    XftDrawStringUtf8(draw, &color, font, 10, text_y, (FcChar8*)status, strlen(status));
    
    XftDrawDestroy(draw);
    XFlush(display);
}

void tile_windows() {
    Desktop *d = &desktops[current_desktop];
    if (d->count == 0) return;

    int sw = DisplayWidth(display, 0);
    int sh = DisplayHeight(display, 0) - BAR_HEIGHT;

    if (d->count == 1 && current_layout == LAYOUT_TILE) {
        // Single window: make it fullscreen
        XMoveResizeWindow(display, d->windows[0], 0, BAR_HEIGHT, sw, sh);
    } else {
        // Multiple windows: use master/stack layout
        int mw = (sw * master_width_ratio) / 100;

        for (int i = 0; i < d->count; i++) {
            if (d->is_floating[i]) {
                // Floating windows are not resized
                continue;
            }

            if (i == 0) { // Master
                XMoveResizeWindow(display, d->windows[i],
                    GAP_SIZE, BAR_HEIGHT + GAP_SIZE,
                    mw - 2 * GAP_SIZE, sh - 2 * GAP_SIZE);
            } else { // Stack
                XMoveResizeWindow(display, d->windows[i],
                    mw + GAP_SIZE, BAR_HEIGHT + GAP_SIZE + (i-1)*(sh/(d->count-1)),
                    sw - mw - 2 * GAP_SIZE, (sh/(d->count-1)) - 2 * GAP_SIZE);
            }
        }
    }

    // Set borders
    for (int i = 0; i < d->count; i++) {
        XSetWindowBorder(display, d->windows[i], 
            (i == focused_window_idx) ? focused_color : normal_color);
        XSetWindowBorderWidth(display, d->windows[i], BORDER_SIZE);
    }
    XFlush(display);
}

void toggle_floating_window() {
    Desktop *d = &desktops[current_desktop];
    if (d->count == 0) return;

    d->is_floating[focused_window_idx] = !d->is_floating[focused_window_idx];
    tile_windows();
}

void kill_focused_window() {
    Desktop *d = &desktops[current_desktop];
    if (d->count == 0) return;

    Window w = d->windows[focused_window_idx];
    XEvent ev = {
        .xclient = {
            .type = ClientMessage,
            .window = w,
            .message_type = XInternAtom(display, "WM_PROTOCOLS", True),
            .format = 32,
            .data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False)
        }
    };
    
    XSendEvent(display, w, False, NoEventMask, &ev);
    XFlush(display);
}

void move_window_to_desktop(int desktop) {
    if (desktop < 0 || desktop >= DESKTOPS || 
        desktops[current_desktop].count == 0) return;

    Desktop *src = &desktops[current_desktop];
    Desktop *dst = &desktops[desktop];
    
    Window w = src->windows[focused_window_idx];
    XUnmapWindow(display, w);

    // Remove from source
    for (int i = focused_window_idx; i < src->count-1; i++)
        src->windows[i] = src->windows[i+1];
    src->count--;

    // Add to destination
    dst->windows[dst->count++] = w;
    switch_desktop(desktop);
}

void handle_keypress(XKeyEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);
    Bool shift = ev->state & ShiftMask;

    if (!(ev->state & MOD_KEY)) return;

    if (keysym >= XK_1 && keysym <= XK_5) {
        if (shift) move_window_to_desktop(keysym - XK_1);
        else switch_desktop(keysym - XK_1);
    }
    else switch(keysym) {
        case XK_Return: spawn_terminal(); break;
        case XK_d: spawn_dmenu(); break;
        case XK_q: kill_focused_window(); break;
        case XK_t: 
            current_layout = LAYOUT_TILE; 
            tile_windows();
            draw_status_bar();
            break;
        case XK_f: 
            current_layout = LAYOUT_FLOAT; 
            tile_windows();
            draw_status_bar();
            break;
        case XK_space:
            toggle_floating_window();
            break;
        case XK_h: 
            if (master_width_ratio > 10) master_width_ratio -= 5;
            tile_windows();
            break;
        case XK_l: 
            if (master_width_ratio < 90) master_width_ratio += 5;
            tile_windows();
            break;
        case XK_j: 
            focused_window_idx = (focused_window_idx + 1) % desktops[current_desktop].count;
            tile_windows();
            break;
        case XK_k: 
            focused_window_idx = (focused_window_idx - 1 + desktops[current_desktop].count) % desktops[current_desktop].count;
            tile_windows();
            break;
        case XK_p: // Quit window manager with ALT+SHIFT+p
            if (shift) {
                XCloseDisplay(display);
                exit(EXIT_SUCCESS);
            }
            break;
    }
}

void manage_window(Window w) {
    Desktop *d = &desktops[current_desktop];
    if (d->count >= MAX_WINDOWS) return;

    // Add window
    d->windows[d->count++] = w;
    XSelectInput(display, w, StructureNotifyMask | FocusChangeMask | KeyPressMask);
    XMapWindow(display, w);
    tile_windows();
}

int main() {
    if (!(display = XOpenDisplay(NULL))) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    // Initialize state
    root = DefaultRootWindow(display);
    bar_color = hex_to_xcolor(BAR_COLOR);
    bar_text_color = hex_to_xcolor(BAR_TEXT_COLOR);
    focused_color = hex_to_xcolor(FOCUSED_COLOR);
    normal_color = hex_to_xcolor(NORMAL_COLOR);
    memset(desktops, 0, sizeof(desktops));
    
    // Set background
    spawn(BACKGROUND_CMD);

    // Ensure keypress events are captured for the root window (global capture)
    XSelectInput(display, root, KeyPressMask);  // This line captures global key events

    // Initial window setup
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);
    XSync(display, False);

    // Now the event loop starts
    XEvent ev;
    while (1) {
        XNextEvent(display, &ev);
        switch (ev.type) {
            case MapRequest:
                manage_window(ev.xmaprequest.window);
                XMapWindow(display, ev.xmaprequest.window);
                break;
            case DestroyNotify:
                for (int i = 0; i < desktops[current_desktop].count; i++) {
                    if (desktops[current_desktop].windows[i] == ev.xdestroywindow.window) {
                        for (int j = i; j < desktops[current_desktop].count-1; j++)
                            desktops[current_desktop].windows[j] = desktops[current_desktop].windows[j+1];
                        desktops[current_desktop].count--;
                        break;
                    }
                }
                tile_windows();
                break;
            case KeyPress:
                update_cpu_usage();
                handle_keypress(&ev.xkey);
                draw_status_bar();
                break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
