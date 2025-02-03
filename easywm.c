#include "config.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <unistd.h>

// Global state
Display *display;
Window root;
Desktop desktops[DESKTOPS];
int current_desktop = 0;
Layout current_layout = LAYOUT_TILE;
int master_width_ratio = 60;
int focused_window_idx = 0;
float cpu_usage = 0;
unsigned long focused_color, normal_color, border_color, bar_color;

// Function prototypes
void switch_desktop(int);
void spawn(const char*);
void draw_status_bar(void);
void tile_windows(void);
void manage_window(Window);
void kill_focused_window(void);
void update_cpu_usage(void);
void spawn_terminal();
void spawn_dmenu();
unsigned long hex_to_xcolor(const char *hex);
int get_text_width(XftFont *font, const char *text);

// Helper functions
unsigned long hex_to_xcolor(const char *hex) {
    int r, g, b;
    sscanf(hex, "#%02x%02x%02x", &r, &g, &b);
    return (r << 16) | (g << 8) | b;
}

int get_text_width(XftFont *font, const char *text) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, font, (const FcChar8 *)text, strlen(text), &extents);
    return extents.xOff;
}

void spawn_terminal() { spawn(TERMINAL); }
void spawn_dmenu() { spawn(DMENU_CMD); }

void spawn(const char *cmd) {
    if (fork() == 0) {
        execlp("/bin/sh", "sh", "-c", cmd, NULL);
        exit(EXIT_SUCCESS);
    }
}

void draw_status_bar() {
    int screen_width = DisplayWidth(display, DefaultScreen(display));

    // Clear bar with custom background color
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
             (1.0 - (double)info.freeram / info.totalram) * 100,
             current_desktop + 1,
             DESKTOPS,
             current_layout == LAYOUT_TILE ? "[]=" : "><>"
    );
    strcat(status, sysinfo);

    // Calculate text width for centering
    XftFont *font = XftFontOpenName(display, 0, FONT);
    int text_width = get_text_width(font, status);
    int x_pos = (screen_width - text_width) / 2;

    // Draw text
    XftDraw *draw = XftDrawCreate(display, root,
                                  DefaultVisual(display, 0), DefaultColormap(display, 0));

    XftColor color;
    XftColorAllocName(display, DefaultVisual(display, 0),
                      DefaultColormap(display, 0), "white", &color);

    XftDrawStringUtf8(draw, &color, font, x_pos, BAR_HEIGHT - 5, (FcChar8*)status, strlen(status));

    XftDrawDestroy(draw);
    XFlush(display);
}

void switch_desktop(int new_desktop) {
    if (new_desktop < 0 || new_desktop >= DESKTOPS) return;

    // Unmap old desktop windows
    for (int i = 0; i < desktops[current_desktop].count; i++)
        XUnmapWindow(display, desktops[current_desktop].windows[i]);

    // Map new desktop windows
    current_desktop = new_desktop;
    for (int i = 0; i < desktops[current_desktop].count; i++)
        XMapWindow(display, desktops[current_desktop].windows[i]);

    XFlush(display);
    draw_status_bar();
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

void tile_windows() {
    Desktop *d = &desktops[current_desktop];
    if (d->count == 0) return;

    int sw = DisplayWidth(display, 0);
    int sh = DisplayHeight(display, 0) - BAR_HEIGHT;
    int mw = (sw * master_width_ratio) / 100;

    for (int i = 0; i < d->count; i++) {
        XWindowChanges wc;

        if (current_layout == LAYOUT_TILE) {
            if (i == 0) { // Master
                wc.x = GAP_SIZE;
                wc.y = BAR_HEIGHT + GAP_SIZE;
                wc.width = mw - 2 * GAP_SIZE;
                wc.height = sh - 2 * GAP_SIZE;
            } else { // Stack
                wc.x = mw + GAP_SIZE;
                wc.y = BAR_HEIGHT + GAP_SIZE + (i - 1) * (sh / (d->count - 1));
                wc.width = sw - mw - 2 * GAP_SIZE;
                wc.height = (sh / (d->count - 1)) - 2 * GAP_SIZE;
            }
        } else { // Float
            wc.x = 0;
            wc.y = BAR_HEIGHT;
            wc.width = sw;
            wc.height = sh;
        }

        XConfigureWindow(display, d->windows[i], CWX | CWY | CWWidth | CWHeight, &wc);
        XSetWindowBorder(display, d->windows[i], (i == focused_window_idx) ? focused_color : normal_color);
        XSetWindowBorderWidth(display, d->windows[i], BORDER_SIZE);
    }
    XFlush(display);
}

void handle_keypress(XKeyEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);
    Bool shift = ev->state & ShiftMask;

    if (!(ev->state & MOD_KEY)) return;

    if (keysym >= XK_1 && keysym <= XK_5) {
        if (shift) move_window_to_desktop(keysym - XK_1);
        else switch_desktop(keysym - XK_1);
    } else {
        switch (keysym) {
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
        }
    }
}

void manage_window(Window w) {
    Desktop *d = &desktops[current_desktop];
    if (d->count >= MAX_WINDOWS) return;

    // Auto-tiling logic
    if (current_layout == LAYOUT_TILE && d->count > 0) {
        // Insert after master window
        for (int i = d->count; i > 1; i--)
            d->windows[i] = d->windows[i - 1];
        d->windows[1] = w;
        focused_window_idx = 1;
    } else {
        d->windows[d->count] = w;
        focused_window_idx = d->count;
    }
    d->count++;

    XSelectInput(display, w, StructureNotifyMask);
    tile_windows();
    draw_status_bar();
}

int main() {
    if (!(display = XOpenDisplay(NULL))) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    // Initialize state
    root = DefaultRootWindow(display);
    focused_color = hex_to_xcolor(FOCUSED_COLOR);
    normal_color = hex_to_xcolor(NORMAL_COLOR);
    border_color = hex_to_xcolor(NORMAL_COLOR); // Border color for all windows
    bar_color = hex_to_xcolor(BORDER_COLOR); // Status bar background color
    memset(desktops, 0, sizeof(desktops));

    // Set background
    spawn(BACKGROUND_CMD);

    // Initial window setup
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask);

    // Event loop
    XEvent ev;
    while (1) {
        XNextEvent(display, &ev);
        if (ev.type == KeyPress)
            handle_keypress(&ev.xkey);
        else if (ev.type == MapRequest)
            manage_window(ev.xmaprequest.window);
    }

    return 0;
}
