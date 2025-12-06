// My own basic window manager

#include "X11/X.h"
#include "X11/cursorfont.h"
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void panic(char *msg){
  puts(msg);
  exit(EXIT_FAILURE);
}

// Put this into a struct eventually

Display *dpy;
Window root;

Window windows[64] = {};
int next_window = 0;
int focused_window = -1;

void grabKey(char *key, unsigned int mod){
  XGrabKey(dpy, XKeysymToKeycode(dpy, XStringToKeysym(key)), mod, root, false, GrabModeAsync, GrabModeAsync);
  XSync(dpy, false);
}

void grabAll(void){
  // Initializes all grabs
  // Move
  XGrabButton(dpy, 1, 0, DefaultRootWindow(dpy), True,
          ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  // Resize
  XGrabButton(dpy, 1, Mod1Mask, DefaultRootWindow(dpy), True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
 
  // Init shortcuts
  grabKey("F1", Mod1Mask);
  grabKey("d", Mod1Mask);
  grabKey("Tab", Mod1Mask);
}

void focusWindow(void); // Focus

void onMapRequest(XMapRequestEvent *e){
  printf("map %lu\n", e->window);

  /*
  int found = 0;
  for (int i = 0; i < next_window; ++i) {
    if (windows[i] == e->window) {
      found = 1;
      break;
    }
  }
  if (!found && next_window < 64) {
    windows[next_window++] = e->window;
  }
  focused_window = -1;
  */

  XMapWindow(dpy, e->window);
  XRaiseWindow(dpy, e->window);
  XSetInputFocus(dpy, e->window, RevertToPointerRoot, CurrentTime);
}

int main(void){
 
  // Init
  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    panic("Unable to open a X display");
  }

  root = DefaultRootWindow(dpy);
  
  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

  Cursor cursor = XCreateFontCursor(dpy, XC_sb_left_arrow);
  XDefineCursor(dpy, root, cursor);

  grabAll();

  XSync(dpy, 0);

  // Main

  XEvent e;
  XButtonEvent start;
  XWindowAttributes attr;

  start.subwindow = None;

  for (;;) {
    XNextEvent(dpy, &e);

    switch (e.type) {
      case ButtonPress:
        XGetWindowAttributes(dpy, start.subwindow, &attr);
				XSetInputFocus(dpy, start.subwindow, RevertToParent, CurrentTime);
        puts("Button pressed!");

        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        XSync(dpy, 0);

        break;

      case KeyPress:
        XGetWindowAttributes(dpy, start.subwindow, &attr);
				XSetInputFocus(dpy, start.subwindow, RevertToParent, CurrentTime);
        puts("Key pressed!");

        KeyCode code = e.xkey.keycode;
        unsigned int mods = e.xkey.state;

        // Move all actions to Mod1Mask if

        if (code == XKeysymToKeycode(dpy, XStringToKeysym("F1")) && (mods & Mod1Mask)) {
          XRaiseWindow(dpy, e.xkey.subwindow);
        }
        if (code == XKeysymToKeycode(dpy, XStringToKeysym("d")) && (mods & Mod1Mask)) {
          // Run application launcher

          if (fork() == 0){
            setsid();
            execlp("rlaunch", "rlaunch", NULL);
            _exit(1);
          }
          grabAll();
          XFlush(dpy);
        }
        if (code == XKeysymToKeycode(dpy, XStringToKeysym("Tab")) && (mods & Mod1Mask)) {
          // Cycle to next window
          if (next_window > 0) {
            focused_window = (focused_window + 1) % next_window;
            XRaiseWindow(dpy, windows[focused_window]);
            XSetInputFocus(dpy, windows[focused_window], RevertToPointerRoot, CurrentTime);
            printf("Switched focus to window %lu\n", windows[focused_window]);
          }
        }

        // Kill window and WM and Raise window
        break;

      case CreateNotify:
        windows[next_window] = e.xcreatewindow.window;
        ++next_window;

        printf("windows %lu\n", windows[next_window]);
        printf("New window %d\n", e.xcreatewindow.type);
        break;

      case MapRequest:
        onMapRequest(&e.xmaprequest);
        break;
     
      default:
        //puts("Unexpected event.");
        break;

      // Destroy notify
    }

    // Temporary location of resizing and moving logic
    if(e.type == ButtonPress && e.xbutton.subwindow != None)
    {
        XGetWindowAttributes(dpy, e.xbutton.subwindow, &attr);
        start = e.xbutton;
    }
    else if(e.type == MotionNotify && start.subwindow != None)
    {
        int xdiff = e.xbutton.x_root - start.x_root;
        int ydiff = e.xbutton.y_root - start.y_root;

        bool is_resize = (start.state & Mod1Mask);
        
        if (!is_resize) {
          XMoveResizeWindow(dpy, start.subwindow,
              attr.x + xdiff,
              attr.y + ydiff,
              attr.width,
              attr.height);
        }else if (is_resize) {
          XMoveResizeWindow(dpy, start.subwindow,
              attr.x,
              attr.y,
              attr.width + xdiff,
              attr.height + ydiff);
        }

    }
    else if(e.type == ButtonRelease)
        start.subwindow = None;

    XSync(dpy, 0);
  }

  XCloseDisplay(dpy);
  return 0;
}

