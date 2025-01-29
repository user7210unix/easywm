#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h> // For status bar text rendering

#define MAX_WINDOWS 10
#define DESKTOPS 5

typedef struct {
    Window windows[MAX_WINDOWS];
    int count;
} Desktop;

typedef enum {
    LAYOUT_TILE,
    LAYOUT_FLOAT
} Layout;

Display *display;
Window root;
Desktop desktops[DESKTOPS];
int current_desktop = 0;
Layout current_layout = LAYOUT_TILE;
int master_width_ratio = 60; // Percentage of screen width for master area

void spawn_terminal() {
    if (fork() == 0) {
        execlp("st", "st", NULL);
        exit(0);
    }
}

void switch_desktop(int new_desktop) {
    for (int i = 0; i < desktops[current_desktop].count; i++) {
        XUnmapWindow(display, desktops[current_desktop].windows[i]);
    }
    for (int i = 0; i < desktops[new_desktop].count; i++) {
        XMapWindow(display, desktops[new_desktop].windows[i]);
    }
    current_desktop = new_desktop;
    XFlush(display);
}

void draw_status_bar() {
    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int bar_height = 20; // Height of the status bar

    // Clear the status bar area
    XSetForeground(display, DefaultGC(display, DefaultScreen(display)), BlackPixel(display, DefaultScreen(display)));
    XFillRectangle(display, root, DefaultGC(display, DefaultScreen(display)), 0, 0, screen_width, bar_height);

    // Prepare text
    char status[100];
    snprintf(status, sizeof(status), "Desktop: %d | Layout: %s | Master Width: %d%%", 
             current_desktop + 1, current_layout == LAYOUT_TILE ? "Tile" : "Float", master_width_ratio);

    // Draw text
    XftDraw *draw = XftDrawCreate(display, root, DefaultVisual(display, DefaultScreen(display)), DefaultColormap(display, DefaultScreen(display)));
    XftColor color;
    XftColorAllocName(display, DefaultVisual(display, DefaultScreen(display)), DefaultColormap(display, DefaultScreen(display)), "white", &color);
    XftFont *font = XftFontOpenName(display, DefaultScreen(display), "monospace-10");
    XftDrawStringUtf8(draw, &color, font, 10, 15, (FcChar8 *)status, strlen(status));

    XftDrawDestroy(draw);
    XFlush(display);
}

void tile_windows() {
    int count = desktops[current_desktop].count;
    if (count == 0) return;

    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    if (current_layout == LAYOUT_TILE) {
        int master_width = (screen_width * master_width_ratio) / 100;
        int stack_width = screen_width - master_width;

        for (int i = 0; i < count; i++) {
            if (i == 0) {
                // Master window
                XMoveResizeWindow(display, desktops[current_desktop].windows[i],
                                  0, 0, master_width, screen_height);
            } else {
                // Stack windows
                XMoveResizeWindow(display, desktops[current_desktop].windows[i],
                                  master_width, (i - 1) * (screen_height / (count - 1)),
                                  stack_width, screen_height / (count - 1));
            }
        }
    } else if (current_layout == LAYOUT_FLOAT) {
        // Floating layout: do not resize windows
    }
    XFlush(display);
}

void focus_next_window() {
    if (desktops[current_desktop].count == 0) return;

    static int focused_window = 0;
    focused_window = (focused_window + 1) % desktops[current_desktop].count;
    XSetInputFocus(display, desktops[current_desktop].windows[focused_window], RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, desktops[current_desktop].windows[focused_window]);
    XFlush(display);
}

void focus_previous_window() {
    if (desktops[current_desktop].count == 0) return;

    static int focused_window = 0;
    focused_window = (focused_window - 1 + desktops[current_desktop].count) % desktops[current_desktop].count;
    XSetInputFocus(display, desktops[current_desktop].windows[focused_window], RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, desktops[current_desktop].windows[focused_window]);
    XFlush(display);
}

void move_window_to_master() {
    if (desktops[current_desktop].count < 2) return;

    // Swap the first window with the second window
    Window temp = desktops[current_desktop].windows[0];
    desktops[current_desktop].windows[0] = desktops[current_desktop].windows[1];
    desktops[current_desktop].windows[1] = temp;

    tile_windows();
}

void move_window_to_stack() {
    if (desktops[current_desktop].count < 2) return;

    // Swap the first window with the last window
    Window temp = desktops[current_desktop].windows[0];
    desktops[current_desktop].windows[0] = desktops[current_desktop].windows[desktops[current_desktop].count - 1];
    desktops[current_desktop].windows[desktops[current_desktop].count - 1] = temp;

    tile_windows();
}

void increase_master_width() {
    if (master_width_ratio < 90) {
        master_width_ratio += 10;
        tile_windows();
        draw_status_bar();
    }
}

void decrease_master_width() {
    if (master_width_ratio > 10) {
        master_width_ratio -= 10;
        tile_windows();
        draw_status_bar();
    }
}

void handle_keypress(XKeyEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);
    if (ev->state & Mod1Mask) { // ALT key
        if (keysym >= XK_1 && keysym <= XK_5) {
            switch_desktop(keysym - XK_1);
            draw_status_bar();
        } else if (keysym == XK_Return) {
            spawn_terminal();
        } else if (keysym == XK_t) {
            current_layout = LAYOUT_TILE;
            tile_windows();
            draw_status_bar();
        } else if (keysym == XK_f) {
            current_layout = LAYOUT_FLOAT;
            tile_windows();
            draw_status_bar();
        } else if (keysym == XK_j) {
            focus_next_window();
        } else if (keysym == XK_k) {
            focus_previous_window();
        } else if (keysym == XK_h) {
            decrease_master_width();
        } else if (keysym == XK_l) {
            increase_master_width();
        } else if (keysym == XK_J) { // Shift + j
            move_window_to_stack();
        } else if (keysym == XK_K) { // Shift + k
            move_window_to_master();
        }
    }
}

void manage_window(Window w) {
    desktops[current_desktop].windows[desktops[current_desktop].count++] = w;
    XSelectInput(display, w, StructureNotifyMask);
    tile_windows();
    draw_status_bar();
}

int main() {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Unable to open X display\n");
        return 1;
    }

    root = DefaultRootWindow(display);
    memset(desktops, 0, sizeof(desktops));

    XSelectInput(display, root, SubstructureRedirectMask | KeyPressMask);

    XEvent ev;
    while (1) {
        XNextEvent(display, &ev);
        switch (ev.type) {
        case KeyPress:
            handle_keypress(&ev.xkey);
            break;
        case MapRequest:
            manage_window(ev.xmaprequest.window);
            XMapWindow(display, ev.xmaprequest.window);
            break;
        case DestroyNotify:
            for (int i = 0; i < desktops[current_desktop].count; i++) {
                if (desktops[current_desktop].windows[i] == ev.xdestroywindow.window) {
                    for (int j = i; j < desktops[current_desktop].count - 1; j++) {
                        desktops[current_desktop].windows[j] = desktops[current_desktop].windows[j + 1];
                    }
                    desktops[current_desktop].count--;
                    break;
                }
            }
            tile_windows();
            draw_status_bar();
            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}
