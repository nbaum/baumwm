#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <map>

Display *dpy;
Window root;

unsigned long hexcolor (const char *s)
{
  XColor color;
  Colormap cm = DefaultColormap(dpy, 0);
  XParseColor(dpy, cm, s, &color);
  XAllocColor(dpy, cm, &color);
  return color.pixel;
}

void parse_modifiers (const char *&spec, int &mask)
{
  mask = 0;
  for (; spec[1] == '-'; spec += 2) {
    switch (spec[0]) {
    case 'S':
      mask |= ShiftMask;
      break;
    case 'C':
      mask |= ControlMask;
      break;
    case 'A':
      mask |= Mod1Mask;
      break;
    case 'M':
      mask |= Mod2Mask;
      break;
    }
  }
}

void parse_key (const char *spec, int &sym, int &mask)
{
  parse_modifiers(spec, mask);
  sym = XStringToKeysym(spec);
}

void parse_button (const char *spec, int &button, int &mask)
{
  parse_modifiers(spec, mask);
  button = atoi(spec);
}

void grab_key (Window w, const char *spec)
{
  int sym, mask;
  parse_key(spec, sym, mask);
  XGrabKey(dpy, sym, mask, w, True, GrabModeAsync, GrabModeAsync);
}

void grab_button (Window w, const char *spec)
{
  int button, mask;
  parse_button(spec, button, mask);
  XGrabButton(dpy, button, mask, w, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
}

void start ()
{
  dpy = XOpenDisplay(0);
  root = DefaultRootWindow(dpy);
  XSelectInput(dpy, root, SubstructureRedirectMask);
}

struct Client
{
  Window frame, child;
  int x, y, width, height, ox, oy;
  XButtonEvent start;
  bool fullscreen;
};

std::map<Window, Client *> clients;

Client *client_for_window (Window w)
{
  Client *c = clients[w];
  if (!c)
    c = clients[w] = new Client { .child = w };
  return c;
}

int main()
{
  start();
  grab_key(root, "A-f");
  grab_button(root, "A-1");
  grab_button(root, "A-2");
  grab_button(root, "A-3");
  
  if (!fork()) {
    execlp("xterm", "xterm", NULL);
    exit(0);
  }
  
  for(;;)
  {
    XEvent ev;
    XNextEvent(dpy, &ev);
    int wf, hf;
    switch (ev.type) {
    case MapRequest:
      {
        auto &client = *client_for_window(ev.xmap.window);
        XSetWindowAttributes attr;
        attr.override_redirect = True;
        attr.border_pixel = hexcolor("#FF0000");
        attr.background_pixel = hexcolor("#FF0000");
        auto w = XCreateWindow(dpy, root, client.x, client.y, client.width, client.height,
                              1, CopyFromParent, InputOutput, CopyFromParent,
                              CWOverrideRedirect | CWBackPixel | CWBorderPixel, &attr);
        XReparentWindow(dpy, ev.xmaprequest.window, w, 0, 0);
        clients[w] = &client;
        client.frame = w;
        XMapWindow(dpy, w);
        XWindowChanges wc = {
          .width = client.width,
          .height = client.height,
          .border_width = 0
        };
        XConfigureWindow(dpy, ev.xmaprequest.window, CWWidth | CWHeight | CWBorderWidth, &wc);
        XMapWindow(dpy, w);
        XMapWindow(dpy, ev.xmaprequest.window);
        break;
      }
    case ConfigureRequest:
      {
        auto &client = *client_for_window(ev.xconfigure.window);
        client.x = ev.xconfigure.x;
        client.y = ev.xconfigure.y;
        client.width = ev.xconfigure.width;
        client.height = ev.xconfigure.height;
        client.fullscreen = false;
        XConfigureEvent xce = {
          .type = ConfigureNotify,
          .event = ev.xconfigure.window,
          .window = ev.xconfigure.window,
          .x = ev.xconfigure.x,
          .y = ev.xconfigure.y,
          .width = ev.xconfigure.width,
          .height = ev.xconfigure.height,
          .border_width = ev.xconfigure.border_width
        };
        XSendEvent(dpy, ev.xconfigure.window, False, StructureNotifyMask, (XEvent *) &xce);
        break;
      }
    case ButtonPress:
      {
        auto &client = *client_for_window(ev.xbutton.subwindow);
        if (ev.xbutton.button == 2) {
          XLowerWindow(dpy, ev.xbutton.subwindow);
        } else {
          client.fullscreen = false;
          XRaiseWindow(dpy, ev.xbutton.subwindow);
          client.start = ev.xbutton;
          client.ox = client.x;
          client.oy = client.oy;
          XGrabPointer(dpy, ev.xbutton.subwindow, True,
                  PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
                  GrabModeAsync, None, None, CurrentTime);
          XWindowAttributes attr;
          XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
          wf = (ev.xbutton.x - attr.x) * 5 / attr.width - 2;
          hf = (ev.xbutton.y - attr.y) * 5 / attr.height - 2;
        }
      }
      break;
    case ButtonRelease:
      XUngrabPointer(dpy, CurrentTime);
      break;
    case MotionNotify:
      {
        while(XCheckTypedEvent(dpy, MotionNotify, &ev));
        auto &client = *client_for_window(ev.xbutton.window);
        int xdiff, ydiff;
        xdiff = (ev.xbutton.x_root - client.start.x_root);
        ydiff = (ev.xbutton.y_root - client.start.y_root);
        client.start.x_root = ev.xbutton.x_root;
        client.start.y_root = ev.xbutton.y_root;
        if (client.start.button == 1) {
          client.x += xdiff;
          client.y += ydiff;
          XMoveResizeWindow(dpy, client.frame, client.x, client.y, client.width, client.height);
        } else if (client.start.button == 3) {
          if (wf < 0) {
            client.x += xdiff;
            client.width -= xdiff;
          } else if (wf > 0) {
            client.width += xdiff;
          }
          if (hf < 0) {
            client.y += ydiff;
            client.height -= ydiff;
          } else if (hf > 0) {
            client.height += ydiff;
          }
          XMoveResizeWindow(dpy, client.frame, client.x, client.y, client.width, client.height);
          XMoveResizeWindow(dpy, client.child, 0, 0, client.width, client.height);
        }        
      }
    }
  }
}
