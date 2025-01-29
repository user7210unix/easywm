#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include <X11/XKBlib.h>

#define MAX_WINDOWS 10
#define DESKTOPS 5

typedef struct {
    Window windows[MAX_WINDOWS];
    int count;
} Desktop;

Display *display;
Window root;
Desktop desktops[DESKTOPS];
int current_desktop = 0;

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

void tile_windows() {
    int count = desktops[current_desktop].count;
    if (count == 0) return;

    int screen_width = DisplayWidth(display, DefaultScreen(display));
    int screen_height = DisplayHeight(display, DefaultScreen(display));

    int window_width = screen_width / count;
    for (int i = 0; i < count; i++) {
        XMoveResizeWindow(display, desktops[current_desktop].windows[i],
                          i * window_width, 0, window_width, screen_height);
    }
    XFlush(display);
}

void handle_keypress(XKeyEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(display, ev->keycode, 0, 0);
    if (ev->state & Mod1Mask) { // ALT key
        if (keysym >= XK_1 && keysym <= XK_5) {
            switch_desktop(keysym - XK_1);
        } else if (keysym == XK_Return) {
            spawn_terminal();
        }
    }
}

void manage_window(Window w) {
    desktops[current_desktop].windows[desktops[current_desktop].count++] = w;
    XSelectInput(display, w, StructureNotifyMask);
    tile_windows();
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
            break;
        }
    }

    XCloseDisplay(display);
    return 0;
}

