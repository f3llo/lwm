#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_CLIENTS 64
#define BORDER_WIDTH 2

typedef struct {
    Window client;
    Window frame;
} ManagedWindow;

Display *dpy;
Window root;
ManagedWindow managed[MAX_CLIENTS];
int managed_count = 0;
int focused = -1;

Atom wm_delete;
Atom wm_protocols;

void panic(const char *msg) {
    fprintf(stderr, "lwm panic: %s\n", msg);
    exit(1);
}

void grabGlobalKeys(Window win) {
    int alt = Mod1Mask;
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("d")), alt, win, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("Tab")), alt, win, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym("r")), alt, win, True, GrabModeAsync, GrabModeAsync);
    XSync(dpy, False);
}

void grabButtons(Window win) {
    // move (left) and resize (alt+left)
    XGrabButton(dpy, 1, 0, win, True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 1, Mod1Mask, win, True,
        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
        GrabModeAsync, GrabModeAsync, None, None);
}

void manageWindow(Window w) {
    if (managed_count == MAX_CLIENTS)
        panic("Too many windows!");

    // Is this already managed?
    for (int i = 0; i < managed_count; ++i) {
        if (managed[i].client == w) return;
    }

    XWindowAttributes attr;
    XGetWindowAttributes(dpy, w, &attr);

    if (attr.override_redirect) {
        XMapWindow(dpy, w);
        return;
    }

    // Make the frame match the client size & position
    Window frame = XCreateSimpleWindow(
        dpy, root,
        attr.x, attr.y, attr.width, attr.height,
        BORDER_WIDTH,
        BlackPixel(dpy, DefaultScreen(dpy)),
        WhitePixel(dpy, DefaultScreen(dpy))
    );

    XSelectInput(dpy, frame,
        SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask |
        ExposureMask | FocusChangeMask | KeyPressMask | PointerMotionMask | StructureNotifyMask
    );

    grabButtons(frame);

    // WM_DELETE_WINDOW, protocols support
    XSetWMProtocols(dpy, w, &wm_delete, 1);

    managed[managed_count].client = w;
    managed[managed_count].frame = frame;
    managed_count++;

    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 0, 0);

    XMapWindow(dpy, frame);
    XMapWindow(dpy, w);
    XRaiseWindow(dpy, frame);

    // Set focus
    focused = managed_count - 1;
    XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
}

int findManagedByFrame(Window frame) {
    for (int i = 0; i < managed_count; ++i)
        if (managed[i].frame == frame)
            return i;
    return -1;
}
int findManagedByClient(Window client) {
    for (int i = 0; i < managed_count; ++i)
        if (managed[i].client == client)
            return i;
    return -1;
}

void removeManaged(int i) {
    if (i < 0 || i >= managed_count) return;
    XDestroyWindow(dpy, managed[i].frame);
    managed[i] = managed[managed_count-1];
    managed_count--;
    if (managed_count == 0) focused = -1;
    else if (focused >= managed_count) focused = managed_count-1;
}

int main(void) {
    dpy = XOpenDisplay(NULL);
    if (!dpy) panic("Unable to open X display");
    root = DefaultRootWindow(dpy);

    wm_delete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wm_protocols = XInternAtom(dpy, "WM_PROTOCOLS", False);

    XSelectInput(dpy, root,
        SubstructureRedirectMask | SubstructureNotifyMask |
        ButtonPressMask | StructureNotifyMask |
        PropertyChangeMask | KeyPressMask
    );
    Cursor cursor = XCreateFontCursor(dpy, XC_sb_left_arrow);
    XDefineCursor(dpy, root, cursor);

    grabGlobalKeys(root);

    XSync(dpy, 0);

    XButtonEvent start;
    start.subwindow = None;
    int is_dragging = 0;
    int drag_managed = -1;
    XWindowAttributes drag_attr;

    for (;;) {
        XEvent e;
        XNextEvent(dpy, &e);

        switch (e.type) {
        case MapRequest:
            manageWindow(e.xmaprequest.window);
            break;
        case ConfigureRequest: {
            XConfigureRequestEvent *cr = &e.xconfigurerequest;
            int idx = findManagedByClient(cr->window);
            Window win_to_config = cr->window;
            if (idx >= 0) win_to_config = managed[idx].frame;
            XWindowChanges wc = {
                .x = cr->x, .y = cr->y,
                .width = cr->width, .height = cr->height,
                .border_width = cr->border_width,
                .sibling = cr->above,
                .stack_mode = cr->detail
            };
            XConfigureWindow(dpy, win_to_config, cr->value_mask, &wc);
            break;
        }
        case ClientMessage:
            if (e.xclient.message_type == wm_protocols &&
                (Atom)e.xclient.data.l[0] == wm_delete) {
                int idx = findManagedByClient(e.xclient.window);
                if (idx >= 0)
                    XDestroyWindow(dpy, managed[idx].client);
            }
            break;
        case DestroyNotify: {
            int idx = findManagedByClient(e.xdestroywindow.window);
            if (idx >= 0)
                removeManaged(idx);
            break;
        }
        case ButtonPress: {
            // move or resize initiated via frame
            int idx = findManagedByFrame(e.xbutton.window);
            if (idx >= 0) {
                drag_managed = idx;
                is_dragging = 1;
                XGetWindowAttributes(dpy, managed[idx].frame, &drag_attr);
                start = e.xbutton;
            }
            break;
        }
        case MotionNotify:
            if (is_dragging && drag_managed >= 0) {
                int xdiff = e.xmotion.x_root - start.x_root;
                int ydiff = e.xmotion.y_root - start.y_root;
                bool is_resize = (start.state & Mod1Mask);

                int new_w = drag_attr.width, new_h = drag_attr.height;
                int new_x = drag_attr.x, new_y = drag_attr.y;

                if (is_resize) {
                    new_w = MAX(1, drag_attr.width + xdiff);
                    new_h = MAX(1, drag_attr.height + ydiff);
                } else {
                    new_x = drag_attr.x + xdiff;
                    new_y = drag_attr.y + ydiff;
                }

                XMoveResizeWindow(dpy, managed[drag_managed].frame,
                    new_x, new_y, new_w, new_h);
                XMoveResizeWindow(dpy, managed[drag_managed].client,
                    0, 0, new_w, new_h);
                XFlush(dpy);
            }
            break;
        case ButtonRelease:
            is_dragging = 0;
            drag_managed = -1;
            start.subwindow = None;
            break;
        case KeyPress:
            // Only handle global keys on root window
            if (e.xkey.window == root) {
                KeyCode code = e.xkey.keycode;
                unsigned int mods = e.xkey.state;
                if ((code == XKeysymToKeycode(dpy, XStringToKeysym("d"))) && (mods & Mod1Mask)) {
                    if (fork() == 0) {
                        setsid();
                        execlp("rlaunch", "rlaunch", NULL);
                        _exit(1);
                    }
                }
                if ((code == XKeysymToKeycode(dpy, XStringToKeysym("Tab"))) && (mods & Mod1Mask)) {
                    // Alt+Tab: cycle focus
                    if (managed_count > 0) {
                        focused = (focused + 1) % managed_count;
                        XRaiseWindow(dpy, managed[focused].frame);
                        XSetInputFocus(dpy, managed[focused].client, RevertToPointerRoot, CurrentTime);
                    }
                }
                if ((code == XKeysymToKeycode(dpy, XStringToKeysym("r"))) && (mods & Mod1Mask)) {
                    // Alt+F1: raise focused window
                    if (focused >= 0 && focused < managed_count) {
                        XRaiseWindow(dpy, managed[focused].frame);
                        XSetInputFocus(dpy, managed[focused].client, RevertToPointerRoot, CurrentTime);
                    }
                }
            }
            break;
        }
        XSync(dpy, 0);
    }
    XCloseDisplay(dpy);
    return 0;
}
