#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>

#include <map>
#include <list>
#include <algorithm>
#include <iostream>

#include "util.h"
#include "display.h"

Display *dpy;
Window root;

Mode::Mode ()
{
  exit_loop = false;
}

void event_loop (Mode *mode)
{
  XEvent event;
  XMaskEvent(dpy, mode->event_mask(), &event);
  while (!mode->exit_loop) {
    switch (event.type) {
    case ConfigureRequest:
      mode->configure_request(event.xconfigurerequest);
      break;
    }
  }
}

void setup ()
{
  dpy = XOpenDisplay(0);
  root = DefaultRootWindow(dpy);
  XSelectInput(dpy, root, SubstructureRedirectMask);
}

unsigned long pixel (const char *s)
{
  XColor color;
  Colormap cm = DefaultColormap(dpy, 0);
  XParseColor(dpy, cm, s, &color);
  XAllocColor(dpy, cm, &color);
  return color.pixel;
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

const char *XEventName (int type)
{
  static const char *event_names[] = {
    [KeyPress] = "KeyPress",
    [KeyRelease] = "KeyRelease",
    [ButtonPress] = "ButtonPress",
    [ButtonRelease] = "ButtonRelease",
    [MotionNotify] = "MotionNotify",
    [EnterNotify] = "EnterNotify",
    [LeaveNotify] = "LeaveNotify",
    [FocusIn] = "FocusIn",
    [FocusOut] = "FocusOut",
    [KeymapNotify] = "KeymapNotify",
    [Expose] = "Expose",
    [GraphicsExpose] = "GraphicsExpose",
    [NoExpose] = "NoExpose",
    [VisibilityNotify] = "VisibilityNotify",
    [CreateNotify] = "CreateNotify",
    [DestroyNotify] = "DestroyNotify",
    [UnmapNotify] = "UnmapNotify",
    [MapNotify] = "MapNotify",
    [MapRequest] = "MapRequest",
    [ReparentNotify] = "ReparentNotify",
    [ConfigureNotify] = "ConfigureNotify",
    [ConfigureRequest] = "ConfigureRequest",
    [GravityNotify] = "GravityNotify",
    [ResizeRequest] = "ResizeRequest",
    [CirculateNotify] = "CirculateNotify",
    [CirculateRequest] = "CirculateRequest",
    [PropertyNotify] = "PropertyNotify",
    [SelectionClear] = "SelectionClear",
    [SelectionRequest] = "SelectionRequest",
    [SelectionNotify] = "SelectionNotify",
    [ColormapNotify] = "ColormapNotify",
    [ClientMessage] = "ClientMessage",
    [MappingNotify] = "MappingNotify",
    [GenericEvent] = "GenericEvent",
  };
  return event_names[type];
}
