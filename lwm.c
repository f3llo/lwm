// My own basic window manager

#include "X11/X.h"
#include "X11/cursorfont.h"
#include <X11/Xlib.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

void panic(char *msg){
  puts(msg);
  exit(EXIT_FAILURE);
}

Display *dpy;
Window root;

Window windows[64] = {};
int next_window = 0;

void launch_rmenu(void){

  XUngrabPointer(dpy, CurrentTime);
  XUngrabKeyboard(dpy, CurrentTime);
  XFlush(dpy);

  if (fork() == 0){
    setsid();
    execlp("rlaunch", "rlaunch", NULL);
    _exit(1);
  }
}

void grabKey(char *key, unsigned int mod){
  KeySym sym = XStringToKeysym(key);
  KeyCode code = XKeysymToKeycode(dpy, sym);
  XGrabKey(dpy, code, mod, root, false, GrabModeAsync, GrabModeAsync);
  XSync(dpy, false);
}

void onCreateNotify(XCreateWindowEvent *e){
  windows[next_window] = e->window;
  printf("windows %lu\n", windows[next_window]);
  ++next_window;
}

void onMapRequest(XMapRequestEvent *e){
  printf("map %lu\n", e->window);
  XMapWindow(dpy, e->window);
  XSetInputFocus(dpy, e->window, RevertToPointerRoot, CurrentTime);
}

//Unmap?
//Change window

void grabAll(void){
  // Initializes all grabs
  // Move
  XGrabButton(dpy, 1, 0, DefaultRootWindow(dpy), True,
          ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  // Resize
  XGrabButton(dpy, 1, Mod1Mask, DefaultRootWindow(dpy), True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
 
  // 'KeyPress' Event when pressing "a" + "Shift"
  grabKey("F1", Mod1Mask);
  grabKey("d", Mod1Mask);
}

int main(void){
  
  dpy = XOpenDisplay(NULL);
  if (dpy == NULL) {
    panic("Unable to open a X display");
  }

  root = DefaultRootWindow(dpy);
  
  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);
  XSync(dpy, 0);

  Cursor cursor = XCreateFontCursor(dpy, XC_sb_left_arrow);
  XDefineCursor(dpy, root, cursor);
  XSync(dpy, 0);

  grabAll();

  XEvent e;
  XButtonEvent start;
  XWindowAttributes attr;

  start.subwindow = None;
  for (;;) {
    XNextEvent(dpy, &e);

    switch (e.type) {
      case ButtonPress:
        XAllowEvents(dpy, ReplayPointer, CurrentTime);
        XSync(dpy, 0);
        puts("Button pressed!");

        break;

      case KeyPress:
        puts("Key pressed!");
        KeyCode code = e.xkey.keycode;
        unsigned int mods = e.xkey.state;

        // e.xbutton.subwindow != None
        if (e.xkey.subwindow != None) {
          if (code == XKeysymToKeycode(dpy, XStringToKeysym("F1")) && (mods & Mod1Mask)) {
            XRaiseWindow(dpy, e.xkey.subwindow);
          }
          if (code == XKeysymToKeycode(dpy, XStringToKeysym("d")) && (mods & Mod1Mask)) {
            // Run application launcher
            launch_rmenu();
            grabAll();
            XFlush(dpy);
          }
        }
        break;

      case CreateNotify:
        onCreateNotify(&e.xcreatewindow);
        printf("New window %d\n", e.xcreatewindow.type);
        break;

      case MapRequest:
        onMapRequest(&e.xmaprequest);
        break;
     
      default:
        puts("Unexpected event.");
        break;
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

