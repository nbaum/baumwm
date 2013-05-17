#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>

#include <map>
#include <list>
#include <algorithm>
#include <iostream>

#include "util.h"

Display *dpy;
Window root;

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

std::map<int, std::list<bool (*)(XEvent)>> listeners;

XEvent memorized_event = { .type = 0 };

XEvent next_event (int mask)
{
  XEvent retval;
  if (memorized_event.type & mask) {
    retval = memorized_event;
    memorized_event.type = 0;
  } else {
    XMaskEvent(dpy, mask, &retval);
  }
  return retval;
}

XEvent next_event_nb ()
{
  XEvent event;
  if (XCheckMaskEvent(dpy, -1, &event))
    return event;
  event.type = 0;
  return event;
}

XEvent chomp_events (XEvent event)
{
  for (;;) {
    auto tmp = next_event_nb();
    if (tmp.type == 0) {
      return event;
    } else if (tmp.type == event.type) {
      event = tmp;
    } else {
      memorized_event = tmp;
      return event;
    }
  }
}

const char *event_names[] = {
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

void event_loop (int mask = -1)
{
  for(;;) {
    auto event = next_event(mask);
    auto handlers = listeners[event.type];
    std::cout << event_names[event.type] << std::endl;
    for (auto i = handlers.begin(); i != handlers.end(); ++i) {
      if (!(*i)(event))
        break;
    }
  }
}

void listen (int type, bool (*handler) (XEvent))
{
  listeners[type].push_front(handler);
}

void unlisten (int type, bool (*handler) (XEvent))
{
  auto &l = listeners[type];
  l.erase(std::find(l.begin(), l.end(), handler));
}
