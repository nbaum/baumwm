#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <map>
#include <err.h>

#include "util.h"

int ModMask = Mod4Mask;

void spawn (const char *command)
{
  if (!fork()) {
    if (!fork()) {
      execlp(command, command, NULL);
      exit(1);
    }
    exit(0);
  }
}

XWindowAttributes attr;
XButtonEvent start;
int hf, vf;
Display *dpy;
Window root, bar;
unsigned int current_desktop_mask;

unsigned int active_frame_pixel;
unsigned int frame_text_pixel;
unsigned int inactive_frame_pixel;

struct XClient
{
  Window frame, child;
  GC gc;
  std::string title;
  int x, y, width, height;
  unsigned int desktop_mask;
};

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
    XWindowAttributes attr;
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, 0, 0);
    c = new XClient { .frame = frame, .child = w, .desktop_mask = current_desktop_mask };
    XSetWindowBorder(dpy, frame, inactive_frame_pixel);
    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 0, 15);
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

void XEventLoop ()
{
  for (;;) {
    XEvent event;
    XNextEvent(dpy, &event);
    std::cerr << XEventName(event.type) << std::endl;
    if (auto fn = event_handlers[event.type])
      fn(event);
  }
}

void XDrawFrame (XClient& client, bool active)
{
  if (active) {
    XSetWindowBorder(dpy, client.frame, active_frame_pixel);
    XSetForeground(dpy, client.gc, active_frame_pixel);
  } else {
    XSetWindowBorder(dpy, client.frame, inactive_frame_pixel);
    XSetForeground(dpy, client.gc, inactive_frame_pixel);
  }
  XFillRectangle(dpy, client.frame, client.gc, 0, 0, client.width, 15);
  XSetForeground(dpy, client.gc, frame_text_pixel);
  XDrawString(dpy, client.frame, client.gc, 2, 12, client.title.c_str(), client.title.length());
}

void update_name (XClient &client)
{
  char *name;
  XFetchName(dpy, client.child, &name);
  client.title = name;
  XFree(name);
  XDrawFrame(client, &client == focused);
}

void focus (XClient& client, Window window = None)
{
  if (focused) {
    XDrawFrame(*focused, false);
  }
  if (&client) {
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, client.frame, &attr);
    if (attr.map_state != IsViewable) return;
    XSetInputFocus(dpy, client.child, RevertToPointerRoot, CurrentTime);
    XDrawFrame(client, true);
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
      if (event.state & ShiftMask) {
        XLowerWindow(dpy, event.subwindow);
      } else {
        XRaiseWindow(dpy, event.subwindow);
      }
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
    .y = y - 15,
    .width = width,
    .height = height
  };
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
  auto &client = XFindClient(event.window, True);
  printf("clients wants to be at %i+%i\n", event.x, event.y);
  if (event.value_mask & CWX) client.x = event.x;
  if (event.value_mask & CWY) client.y = event.y;
  if (event.value_mask & CWWidth) client.width = event.width;
  if (event.value_mask & CWHeight) client.height = event.height;
  move_resize(client, client.x, client.y, client.width, client.height);
  XSetWindowBorderWidth(dpy, client.child, 0);
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
  XWindowAttributes attr;
  XGetWindowAttributes(dpy, event.window, &attr);
  move_resize(client, attr.x, attr.y, attr.width, attr.height + 15);
  XMapWindow(dpy, event.window);
  XMapWindow(dpy, client.frame);
  //XSetWMState(client, 1);
  XSetWindowBorderWidth(dpy, client.child, 0);
  update_name(client);
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
  if (&client)
    XDrawFrame(client, &client == focused);
}

void property (XPropertyEvent& event)
{
  auto &client = XFindClient(event.window, False);
  std::cout << XGetAtomName(dpy, event.atom) << std::endl;
  if (!&client) return;
  update_name(client);
}

void create_bar ()
{
  bar = XCreateWindow(dpy, root,
                      0, DisplayHeight(dpy, 0) - 15,
                      DisplayWidth(dpy, 0), 15,
                      1,
                      CopyFromParent,
                      InputOutput,
                      CopyFromParent,
                      0, 0);
  XSetWindowBackground(dpy, bar, inactive_frame_pixel);
  XSetWindowBorderWidth(dpy, bar, 0);
  XMapWindow(dpy, bar);
  XSelectInput(dpy, bar, ExposureMask);
}

void set_desktop (unsigned int mask)
{
  printf("dmask %i -> %i\n", current_desktop_mask, mask);
  current_desktop_mask = mask;
  for (auto i = clients.begin(); i != clients.end(); ++i) {
    auto &client = *i->second;
    if (&client) {
      if (client.desktop_mask & mask) {
        XMapWindow(dpy, client.frame);
      } else {
        XUnmapWindow(dpy, client.frame);
      }
    }
  }
}

void set_desktop (XClient& client, unsigned int mask)
{
  if (!&client) return;
  client.desktop_mask = mask;
  set_desktop(current_desktop_mask);
}

void flip_desktop (unsigned int mask)
{
  set_desktop(current_desktop_mask ^ mask);
}

void flip_desktop (XClient& client, unsigned int mask)
{
  if (!&client) return;
  set_desktop(client, client.desktop_mask ^ mask);
}

void key_press (XKeyPressedEvent& event)
{
  auto &client = XFindClient(event.subwindow, False);
  if (match_key(event, "M-1")) {
    set_desktop(1);
  } else if (match_key(event, "M-2")) {
    set_desktop(2);
  } else if (match_key(event, "M-3")) {
    set_desktop(4);
  } else if (match_key(event, "M-4")) {
    set_desktop(8);
  } else if (match_key(event, "M-5")) {
    set_desktop(16);
  } else if (match_key(event, "M-6")) {
    set_desktop(32);
  } else if (match_key(event, "M-7")) {
    set_desktop(64);
  } else if (match_key(event, "M-8")) {
    set_desktop(128);
  } else if (match_key(event, "M-9")) {
    set_desktop(256);
  } else if (match_key(event, "M-S-1")) {
    set_desktop(client, 1);
  } else if (match_key(event, "M-S-2")) {
    set_desktop(client, 2);
  } else if (match_key(event, "M-S-3")) {
    set_desktop(client, 4);
  } else if (match_key(event, "M-S-4")) {
    set_desktop(client, 8);
  } else if (match_key(event, "M-S-5")) {
    set_desktop(client, 16);
  } else if (match_key(event, "M-S-6")) {
    set_desktop(client, 32);
  } else if (match_key(event, "M-S-7")) {
    set_desktop(client, 64);
  } else if (match_key(event, "M-S-8")) {
    set_desktop(client, 128);
  } else if (match_key(event, "M-S-9")) {
    set_desktop(client, 256);
  } else if (match_key(event, "M-C-S-1")) {
    flip_desktop(client, 1);
  } else if (match_key(event, "M-C-S-2")) {
    flip_desktop(client, 2);
  } else if (match_key(event, "M-C-S-3")) {
    flip_desktop(client, 4);
  } else if (match_key(event, "M-C-S-4")) {
    flip_desktop(client, 8);
  } else if (match_key(event, "M-C-S-5")) {
    flip_desktop(client, 16);
  } else if (match_key(event, "M-C-S-6")) {
    flip_desktop(client, 32);
  } else if (match_key(event, "M-C-S-7")) {
    flip_desktop(client, 64);
  } else if (match_key(event, "M-C-S-8")) {
    flip_desktop(client, 128);
  } else if (match_key(event, "M-C-S-9")) {
    flip_desktop(client, 256);
  } else if (match_key(event, "M-C-1")) {
    flip_desktop(1);
  } else if (match_key(event, "M-C-2")) {
    flip_desktop(2);
  } else if (match_key(event, "M-C-3")) {
    flip_desktop(4);
  } else if (match_key(event, "M-C-4")) {
    flip_desktop(8);
  } else if (match_key(event, "M-C-5")) {
    flip_desktop(16);
  } else if (match_key(event, "M-C-6")) {
    flip_desktop(32);
  } else if (match_key(event, "M-C-7")) {
    flip_desktop(64);
  } else if (match_key(event, "M-C-8")) {
    flip_desktop(128);
  } else if (match_key(event, "M-C-9")) {
    flip_desktop(256);
  } else if (match_key(event, "M-Return")) {
    spawn(getenv("TERMINAL"));
  } else if (match_key(event, "M-d")) {
    static unsigned int old_desktop_mask = 0;
    unsigned int tmp = current_desktop_mask;
    set_desktop(old_desktop_mask);
    old_desktop_mask = tmp;
  } else if (match_key(event, "M-c")) {
    XDestroyClient(client.child);
  }
}

int error (Display *dpy, XErrorEvent *error)
{
  return 0;
}

int main ()
{
  if (!(dpy = XOpenDisplay(0)))
    err(1, "failed to start");
  XSynchronize(dpy, True);
  current_desktop_mask = 1;
  active_frame_pixel   = XMakeColor(dpy, "rgb:f/0/0");
  frame_text_pixel     = XMakeColor(dpy, "rgb:f/f/f");
  inactive_frame_pixel = XMakeColor(dpy, "rgb:8/8/8");
  //system("killall xterm");
  //spawn("xterm");
  XA_NET_WM_STATE = XInternAtom(dpy, "_NET_WM_STATE", False);
  root = DefaultRootWindow(dpy);
  XSetErrorHandler(error);
  Window *children, parent;
  unsigned int nchildren;
  XQueryTree(dpy, root, &root, &parent, &children, &nchildren);
  for (int j = 0; j < nchildren; j++) {
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, children[j], &attr);
    if (!attr.override_redirect && attr.map_state == IsViewable) {
      auto &client = XFindClient(children[j], True);
      XMoveResizeWindow(dpy, client.frame, attr.x - 1, attr.y - 16, attr.width, attr.height + 15);
      client.x = attr.x - 1;
      client.y = attr.y - 16;
      client.width = attr.width;
      client.height = attr.height + 15;
      XSetWindowBorderWidth(dpy, client.child, 0);
      XMapWindow(dpy, client.child);
      XMapWindow(dpy, client.frame);
      XClearWindow(dpy, client.frame);
      update_name(client);
    }
  }
  XSelectInput(dpy, root, KeyPressMask | SubstructureRedirectMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
  XSetWindowBackground(dpy, root, XMakeColor(dpy, "rgb:4/6/8"));
  XClearWindow(dpy, root);
  grab_key(dpy, root, "M-Return");
  grab_key(dpy, root, "M-d");
  grab_key(dpy, root, "M-c");
  grab_key(dpy, root, "M-1");
  grab_key(dpy, root, "M-2");
  grab_key(dpy, root, "M-3");
  grab_key(dpy, root, "M-4");
  grab_key(dpy, root, "M-5");
  grab_key(dpy, root, "M-6");
  grab_key(dpy, root, "M-7");
  grab_key(dpy, root, "M-8");
  grab_key(dpy, root, "M-9");
  grab_key(dpy, root, "M-S-1");
  grab_key(dpy, root, "M-S-2");
  grab_key(dpy, root, "M-S-3");
  grab_key(dpy, root, "M-S-4");
  grab_key(dpy, root, "M-S-5");
  grab_key(dpy, root, "M-S-6");
  grab_key(dpy, root, "M-S-7");
  grab_key(dpy, root, "M-S-8");
  grab_key(dpy, root, "M-S-9");
  grab_key(dpy, root, "C-M-1");
  grab_key(dpy, root, "C-M-2");
  grab_key(dpy, root, "C-M-3");
  grab_key(dpy, root, "C-M-4");
  grab_key(dpy, root, "C-M-5");
  grab_key(dpy, root, "C-M-6");
  grab_key(dpy, root, "C-M-7");
  grab_key(dpy, root, "C-M-8");
  grab_key(dpy, root, "C-M-9");
  grab_key(dpy, root, "C-M-S-1");
  grab_key(dpy, root, "C-M-S-2");
  grab_key(dpy, root, "C-M-S-3");
  grab_key(dpy, root, "C-M-S-4");
  grab_key(dpy, root, "C-M-S-5");
  grab_key(dpy, root, "C-M-S-6");
  grab_key(dpy, root, "C-M-S-7");
  grab_key(dpy, root, "C-M-S-8");
  grab_key(dpy, root, "C-M-S-9");
  grab_button(dpy, root, "M-1");
  grab_button(dpy, root, "M-2");
  grab_button(dpy, root, "M-S-2");
  grab_button(dpy, root, "M-3");
  XSetEventHandler(KeyPress, key_press);
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
