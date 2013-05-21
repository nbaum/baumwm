#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <map>
#include <err.h>
#include <sys/time.h>
#include <time.h>

#include "util.h"

void spawn (const char *command)
{
  if (!fork()) {
    execlp(command, command, NULL);
    exit(1);
  }
}

XWindowAttributes attr;
XButtonEvent start;
int hf, vf;
Display *dpy;
Window root;

struct XClient
{
  Window frame, child;
  GC gc;
  std::string title;
  int x, y, width, height;
  int64_t last_exposed;
  bool dirty;
};

int64_t time ()
{
  timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec * 1000000 + tv.tv_usec;
}

std::map<Window, XClient *> clients;

XClient *focused;

void XDestroyClient (Window w)
{
  auto c = clients[w];
  if (!c) return;
  XDestroyWindow(dpy, c->frame);
  clients.erase(c->frame);
  clients.erase(c->child);
  delete c;
}

XClient& XFindClient (Window w, bool create)
{
  auto c = clients[w];
  if (!c && create) {
    XSetWindowAttributes attr;
    attr.backing_store = Always;
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, CWBackingStore, &attr);
    c = new XClient { .frame = frame, .child = w };
    XSetWindowBorder(dpy, frame, XMakeColor(dpy, "rgb:8/8/8"));
    XReparentWindow(dpy, w, frame, 0, 0);
    c->gc = XCreateGC(dpy, w, 0, 0);
    XSetFont(dpy, c->gc, XLoadFont(dpy, "-misc-*-*-*-*-*-12-*-*-*-*-*-*-*"));
    XSelectInput(dpy, frame, ExposureMask | EnterWindowMask | SubstructureNotifyMask | SubstructureRedirectMask);
    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    clients[frame] = clients[w] = c;
  }
  return *c;
}

typedef void (* XEventHandler)(XEvent& event);
XEventHandler event_handlers[LASTEvent];

template<typename T>
inline void XSetEventHandler(int event, void (* fn) (T& event)) {
  event_handlers[event] = reinterpret_cast<XEventHandler>(fn);
}

void XDrawFrame (XClient& client)
{
  if (&client == focused) {
    XSetForeground(dpy, client.gc, XMakeColor(dpy, "rgb:2/4/f"));
  } else {
    XSetForeground(dpy, client.gc, XMakeColor(dpy, "rgb:8/8/8"));
  }
  XFillRectangle(dpy, client.frame, client.gc, 0, 0, client.width, 15);
  XSetForeground(dpy, client.gc, XMakeColor(dpy, "white"));
  XDrawString(dpy, client.frame, client.gc, 2, 12, client.title.c_str(), client.title.length());
  client.dirty = false;
}

void XIdle ()
{
  for (auto i = clients.begin(); i != clients.end(); ++i) {
    if (i->second->dirty)
      XDrawFrame(*i->second);
  }
}

void XEventLoop ()
{
  for (;;) {
    XEvent event;
    XNextEvent(dpy, &event);
    if (auto fn = event_handlers[event.type])
      fn(event);
    timespec t = { .tv_sec = 0, .tv_nsec = 10000 };
    nanosleep(&t, 0);
    if (!XPending(dpy))
      XIdle();
  }
}

void focus (XClient& client, Window window = None)
{
  if (focused) {
    XSetWindowBorder(dpy, focused->frame, XMakeColor(dpy, "rgb:8/8/8"));
    //XSetWindowBackground(dpy, focused->frame, XMakeColor(dpy, "rgb:8/8/8"));
    focused->dirty = true;
  }
  if (&client) {
    XSetWindowBorder(dpy, client.frame, XMakeColor(dpy, "rgb:2/4/f"));
    //XSetWindowBackground(dpy, client.frame, XMakeColor(dpy, "rgb:f/0/0"));
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, client.frame, &attr);
    if (attr.map_state != IsViewable) return;
    XSetInputFocus(dpy, client.child, RevertToPointerRoot, CurrentTime);
    client.dirty = true;
  } else if (window) {
    XSetInputFocus(dpy, window, RevertToPointerRoot, CurrentTime);
  }
  focused = &client;
}

void button_press (XButtonPressedEvent& event)
{
  if (event.window == root) {
    if (event.button == 1) {
      XGetWindowAttributes(dpy, event.subwindow, &attr);
      XGrabPointer(dpy, event.subwindow, True,
                  ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                  GrabModeAsync, root, XCreateFontCursor(dpy, XC_fleur),
                  event.time);
      start = event;
    } else if (event.button == 2) {
      XLowerWindow(dpy, event.subwindow);
    } else {
      XGetWindowAttributes(dpy, event.subwindow, &attr);
      start = event;
      hf = 3 * (start.x - attr.x) / attr.width - 1;
      vf = 3 * (start.y - attr.y) / attr.height - 1;
      int cursors[] = {XC_top_left_corner, XC_top_side, XC_top_right_corner, XC_left_side, XC_circle, XC_right_side, XC_bottom_left_corner, XC_bottom_side, XC_bottom_right_corner};
      int cursor = cursors[4 + hf + 3 * vf];
      start.x = start.x_root;
      start.y = start.y_root;
      if (hf < 0)  start.x_root = attr.x;
      if (hf > 0)  start.x_root = attr.x + attr.width;
      if (vf < 0)  start.y_root = attr.y;
      if (vf > 0)  start.y_root = attr.y + attr.height;
      XWarpPointer(dpy, None, root, 0, 0, 0, 0, start.x_root, start.y_root);
      XGrabPointer(dpy, event.subwindow, True,
                  ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                  GrabModeAsync, root, XCreateFontCursor(dpy, cursor),
                  event.time);
    }
  } else {
    auto &client = XFindClient(event.window, False);
    focus(client);
  }
}

void move_resize (XClient& client, int x, int y, int width, int height)
{
  XMoveResizeWindow(dpy, client.frame, x, y, width, height + 15);
  XMoveResizeWindow(dpy, client.child, 0, 15, width, height);
  XConfigureEvent ev = {
    .type = ConfigureNotify,
    .event = client.child,
    .window = client.child,
    .x = x,
    .y = y + 15,
    .width = width,
    .height = height,
    .border_width = 0,
    .above = None,
    .override_redirect = 0
  };
  XSetWindowBorderWidth(dpy, client.child, 0);
  XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
  client.x = x;
  client.y = y;
  client.width = width;
  client.height = height + 15;
}

void motion (XMotionEvent& event)
{
  auto &client = XFindClient(start.subwindow, False);
  if (!&client) return;
  if (start.button == 1) {
    XWindowAttributes rattr;
    XGetWindowAttributes(dpy, root, &rattr);
    auto dx = event.x_root - start.x_root;
    auto dy = event.y_root - start.y_root;
    if (event.x_root < start.x_root)
      dx = (1.0 * event.x_root / start.x_root) * (attr.x + attr.width) - attr.width;
    else
      dx = (1.0 - 1.0 * (rattr.width - event.x_root) / (rattr.width - start.x_root)) * (rattr.width - attr.x) + attr.x;
    if (event.y_root < start.y_root)
      dy = (1.0 * event.y_root / start.y_root) * (attr.y + attr.height) - attr.height;
    else
      dy = (1.0 - 1.0 * (rattr.height - event.y_root) / (rattr.height - start.y_root)) * (rattr.height - attr.y) + attr.y;
    move_resize(client, dx, dy, attr.width, attr.height - 15);
  } else if (start.button == 3) {
    auto dx = event.x_root - start.x_root;
    auto dy = event.y_root - start.y_root;
    auto dw = 0, dh = 0;
    if (hf < 0) dw -= dx;
    else if (hf > 0) dw = dx, dx = 0;
    else dx = 0;
    if (vf < 0) dh -= dy;
    else if (vf > 0) dh = dy, dy = 0;
    else dy = 0;
    move_resize(client, attr.x + dx, attr.y + dy, std::max(15, attr.width + dw), std::max(15, attr.height + dh));
  }
}

void button_release (XButtonReleasedEvent& event)
{
  XUngrabPointer(dpy, event.time);
  XWarpPointer(dpy, None, root, 0, 0, 0, 0, start.x + (event.x_root - start.x_root), start.y + (event.y_root - start.y_root));
}

void configure (XConfigureRequestEvent& event)
{
  printf("Client wants to move to: %ix%i+%i+%i\n", event.width, event.height, event.x, event.y);
  auto &client = XFindClient(event.window, True);
  move_resize(client, event.x, event.y, event.width, event.height);
}

void XSetWMState (XClient& client, int state)
{
  long data[] = { state, None };
  XChangeProperty(dpy, client.child, XInternAtom(dpy, "WM_STATE", False),
                  XInternAtom(dpy, "WM_STATE", False), 32,
                  PropModeReplace, (unsigned char *) data, 2);
}

void map (XMapRequestEvent& event)
{
  auto &client = XFindClient(event.window, True);
  XGetWindowAttributes(dpy, event.window, &attr);
  XMoveResizeWindow(dpy, client.frame, attr.x, attr.y, attr.width, attr.height);
  XMapWindow(dpy, event.window);
  XMapWindow(dpy, client.frame);
  XSetWMState(client, 1);
}

void destroy (XDestroyWindowEvent& event)
{
  XDestroyClient(event.window);
}

void unmap (XUnmapEvent& event)
{
  auto &client = XFindClient(event.window, False);
  if (&client) {
    XUnmapWindow(dpy, client.frame);
    //XSetWMState(client, 0);
    if (&client == focused)
      focused = 0;
  }
}

void enter (XEnterWindowEvent& event)
{
  auto &client = XFindClient(event.window, false);
  focus(client, event.window);
}

Atom XA_NET_WM_STATE;

void message (XClientMessageEvent& event)
{
  if (event.message_type == XA_NET_WM_STATE) {
    for (int i = 0; i < 5; ++i)
      if (event.data.l[i])
        std::cerr << XGetAtomName(dpy, event.data.l[i]) << std::endl;
  } else {
    std::cerr << XGetAtomName(dpy, event.message_type) << std::endl;
  }
}

void expose (XExposeEvent& event)
{
  auto &client = XFindClient(event.window, False);
  if (!&client) return;
  client.dirty = true;
}

void property (XPropertyEvent& event)
{
  auto &client = XFindClient(event.window, False);
  if (!&client) return;
  std::string atom = XGetAtomName(dpy, event.atom);
  if (atom == "WM_NAME") {
    char *name;
    XFetchName(dpy, client.child, &name);
    client.title = name;
    XFree(name);
    client.dirty = true;
  }
}

int main ()
{
  if (!(dpy = XOpenDisplay(0)))
    err(1, "failed to start");
  XSynchronize(dpy, True);
  system("killall xterm");
  spawn("xterm");
  XA_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
  root = DefaultRootWindow(dpy);
  XSelectInput(dpy, root, SubstructureRedirectMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
  XSetWindowBackground(dpy, root, XMakeColor(dpy, "rgb:4/6/8"));
  XClearWindow(dpy, root);
  grab_button(dpy, root, "A-1");
  grab_button(dpy, root, "A-2");
  grab_button(dpy, root, "A-3");
  XSetEventHandler(Expose, expose);
  XSetEventHandler(ButtonPress, button_press);
  XSetEventHandler(MotionNotify, motion);
  XSetEventHandler(ButtonRelease, button_release);
  XSetEventHandler(ConfigureRequest, configure);
  XSetEventHandler(MapRequest, map);
  XSetEventHandler(UnmapNotify, unmap);
  XSetEventHandler(DestroyNotify, destroy);
  XSetEventHandler(EnterNotify, enter);
  XSetEventHandler(ClientMessage, message);
  XSetEventHandler(PropertyNotify, property);
  XEventLoop();
  return 0;
}
