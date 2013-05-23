#include <X11/extensions/Xinerama.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <map>
#include <err.h>

#include "util.h"

int screen_count;
XineramaScreenInfo *screens;

XineramaScreenInfo *find_screen (int x, int y)
{
  for (int i = 0; i < screen_count; ++i) {
    if (x < screens[i].x_org) continue; 
    if (y < screens[i].y_org) continue; 
    if (x > screens[i].x_org + screens[i].width) continue; 
    if (y > screens[i].y_org + screens[i].height) continue;
    return &screens[i]; 
  }
  return 0;
}

const char *command;

const int HeadlineHeight = 20;

int ModMask = Mod4Mask;

void spawn (const char *command) {
  if (!fork()) {
    execlp("/bin/sh", "/bin/sh", "-c", command, NULL);
    exit(1);
  }
}

XWindowAttributes attr;
XButtonEvent start;
int hf, vf;
Display *dpy;
Window root, bar;
uint32_t current_desktop;

unsigned int active_frame_pixel;
unsigned int frame_text_pixel;
unsigned int inactive_frame_pixel;

struct Geom : XRectangle
{
  bool fullscreen;
};

struct XClient
{
  Window frame, child;
  GC gc;
  std::string title;
  int x, y, width, height;
  uint32_t desktop;
  XSizeHints hints;
  std::map<int, Geom> geom;
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

void ProcessHints (XClient& client)
{
  long mask;
  XGetWMNormalHints(dpy, client.child, &client.hints, &mask);
  if ((client.hints.flags & PMinSize)== 0)
    client.hints.min_height = client.hints.min_width = 0;
  if ((client.hints.flags & PMaxSize) == 0)
    client.hints.max_height = client.hints.max_width = 10000;
  if ((client.hints.flags & PBaseSize) == 0)
    client.hints.base_height = client.hints.min_height,
    client.hints.base_width = client.hints.min_width;
  if ((client.hints.flags & PResizeInc) == 0)
    client.hints.width_inc = client.hints.height_inc = 1;
  auto &h = client.hints;
  //printf("min: %ix%i\n", h.min_width, h.min_height);
  //printf("base: %ix%i\n", h.base_width, h.base_height);
  //printf("size: %ix%i\n", h.width, h.height);
}

XClient& XFindClient (Window w, bool create)
{
  auto c = clients[w];
  if (!c && create) {
    XWindowAttributes attr;
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, 0, 0);
    c = new XClient { .frame = frame, .child = w, .desktop = current_desktop };
    XSetWindowBorder(dpy, frame, BlackPixel(dpy, 0));
    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 4, HeadlineHeight);
    int num;
    Atom *atoms = XListProperties(dpy, w, &num);
    c->gc = XCreateGC(dpy, w, 0, 0);
    XSetFont(dpy, c->gc, XLoadFont(dpy, "-misc-*-*-*-*-*-12-*-*-*-*-*-*-*"));
    XSelectInput(dpy, frame, ButtonPressMask | ExposureMask | EnterWindowMask | SubstructureNotifyMask | SubstructureRedirectMask);
    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    ProcessHints(*c);
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
    printf("Event: %s\n", XEventName(event.type));
    if (auto fn = event_handlers[event.type])
      fn(event);
  }
}

void XDrawFrame (XClient& client, bool active)
{
  unsigned int color;
  if (active)
    color = active_frame_pixel;
  else
    color = inactive_frame_pixel;
  XSetForeground(dpy, client.gc, color);
  XFillRectangle(dpy, client.frame, client.gc, 0, 0,
                 client.width + 8, client.height + HeadlineHeight + 4);
  XSetForeground(dpy, client.gc, BlackPixel(dpy, 0));
  XFillRectangle(dpy, client.frame, client.gc, 3, 3, client.width + 2, client.height + HeadlineHeight - 2);
  XSetForeground(dpy, client.gc, color);
  XFillRectangle(dpy, client.frame, client.gc, 4, 4, client.width, 15);
  XSetForeground(dpy, client.gc, frame_text_pixel);
  XDrawString(dpy, client.frame, client.gc, 8, 15, client.title.c_str(), client.title.length());
}

void update_name (XClient &client)
{
  char *name;
  XFetchName(dpy, client.child, &name);
  if (name) {
    client.title = name;
    XFree(name);
  } else {
    client.title = "Untitled Window";
  }
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
  Window win = event.subwindow ? event.subwindow : event.window;
  auto &client = XFindClient(win, False);
  if (event.button == 1) {
    XGetWindowAttributes(dpy, win, &attr);
    if (&client) {
      attr.x = client.x;
      attr.y = client.y;
      attr.width = client.width;
      attr.height = client.height;
    }
    XGrabPointer(dpy, win, True,
                ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                GrabModeAsync, root, XCreateFontCursor(dpy, XC_fleur),
                event.time);
    start = event;
  } else if (event.button == 2) {
    if (event.state & ShiftMask) {
      XLowerWindow(dpy, win);
    } else {
      XRaiseWindow(dpy, win);
    }
  } else {
    XGetWindowAttributes(dpy, win, &attr);
    if (&client) {
      attr.x = client.x;
      attr.y = client.y;
      attr.width = client.width;
      attr.height = client.height;
    }
    start = event;
    hf = std::max(-1, std::min(1, 3 * (start.x - attr.x) / attr.width - 1));
    vf = std::max(-1, std::min(1, 3 * (start.y - attr.y) / attr.height - 1));
    int cursors[] = {XC_top_left_corner, XC_top_side, XC_top_right_corner, XC_left_side, XC_circle, XC_right_side, XC_bottom_left_corner, XC_bottom_side, XC_bottom_right_corner};
    int cursor = cursors[4 + hf + 3 * vf];
    start.x = start.x_root;
    start.y = start.y_root;
    /*if (hf < 0)  start.x_root = attr.x;
    if (hf > 0)  start.x_root = attr.x + attr.width;
    if (vf < 0)  start.y_root = attr.y;
    if (vf > 0)  start.y_root = attr.y + attr.height;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, start.x_root, start.y_root);*/
    XGrabPointer(dpy, win, True,
                ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                GrabModeAsync, root, XCreateFontCursor(dpy, cursor),
                event.time);
  }
}

void move_resize (XClient& client, int x, int y, int width, int height)
{
  //printf("%ix%i+%i+%i\n", width, height, x, y);
  width = std::min(std::max(std::max(15, client.hints.min_width), width), client.hints.max_width);
  height = std::min(std::max(std::max(15, client.hints.min_height), height), client.hints.max_width);
  width = width - (width - client.hints.base_width) % client.hints.width_inc;
  height = height - (height - client.hints.base_height) % client.hints.height_inc;
  XMoveResizeWindow(dpy, client.frame, x, y, width + 8, height + HeadlineHeight + 4);
  XResizeWindow(dpy, client.child, width, height);
  XConfigureEvent ev = {
    .type = ConfigureNotify,
    .event = client.child,
    .window = client.child,
    .x = x + 4,
    .y = y + HeadlineHeight,
    .width = width,
    .height = height
  };
  XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
  client.x = x;
  client.y = y;
  client.width = width;
  client.height = height;
}

void motion (XMotionEvent& event)
{
  auto &client = XFindClient(start.subwindow ? start.subwindow : start.window, False);
  if (!&client) return;
  if (start.button == 1) {
    XWindowAttributes rattr;
    XGetWindowAttributes(dpy, root, &rattr);
    auto dx = event.x_root - start.x_root;
    auto dy = event.y_root - start.y_root;
    move_resize(client, attr.x + dx, attr.y + dy, attr.width, attr.height);
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
    move_resize(client, attr.x + dx, attr.y + dy, std::max(HeadlineHeight, attr.width + dw), std::max(HeadlineHeight, attr.height + dh));
  }
}

void button_release (XButtonReleasedEvent& event)
{
  XUngrabPointer(dpy, event.time);
  //if (event.button == 3)
  //  XWarpPointer(dpy, None, root, 0, 0, 0, 0, start.x + (event.x_root - start.x_root), start.y + (event.y_root - start.y_root));
}

void configure (XConfigureRequestEvent& event)
{
  auto &client = XFindClient(event.window, True);
  printf("client wants to be ");
  if (event.value_mask & CWWidth)
    printf("%i", event.width);
  if (event.value_mask & CWHeight)
    printf("x%i", event.height);
  if (event.value_mask & (CWX | CWY))
    printf("+");
  if (event.value_mask & CWX)
    printf("%i", event.x);
  if (event.value_mask & CWY)
    printf("+%i", event.y);
  printf("\n");
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
  move_resize(client, client.x, client.y, client.width, client.height + 15);
  XMapWindow(dpy, client.child);
  XMapWindow(dpy, client.frame);
  XSetWMState(client, 1);
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
    XSetWMState(client, 0);
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
        ;
        //std::cerr << XGetAtomName(dpy, event.data.l[i]) << std::endl;
  } else {
    //std::cerr << XGetAtomName(dpy, event.message_type) << std::endl;
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
  //std::cout << XGetAtomName(dpy, event.atom) << std::endl;
  if (!&client) return;
  update_name(client);
}

template<typename T, typename T2>
void setprop (Window w, const char *name, const T2 &value)
{
  printf("Setting %s to %i\n", name, value);
  auto atom = XInternAtom(dpy, name, True);
  T temp = value;
  XChangeProperty(dpy, w, atom, atom, 8, PropModeReplace, (unsigned char *) &temp, sizeof(temp));
}

template<typename T, typename T2>
T getprop (Window w, const char *name, const T2 def)
{
  auto atom = XInternAtom(dpy, name, True);
  T *prop, val;
  Atom type;
  int format;
  unsigned long items, bytes;
  XGetWindowProperty(dpy, w, atom, 0, sizeof(T) / 4 + 1,
                     False, AnyPropertyType,
                     &type, &format, &items, &bytes,
                     (unsigned char **) &prop);
  val = prop ? *prop : def;
  if (prop) XFree(prop);
  return val;
}

void set_desktop (uint32_t num)
{
  printf("dmask %i -> %i\n", current_desktop, num);
  current_desktop = num;
  for (auto i = clients.begin(); i != clients.end(); ++i) {
    auto &client = *i->second;
    if (&client) {
      if (client.desktop == -1 || client.desktop == num) {
        XMapWindow(dpy, client.frame);
      } else {
        XUnmapWindow(dpy, client.frame);
      }
    }
  }
  setprop<long>(root, "_NET_CURRENT_DESKTOP", num);
}

void set_desktop (XClient& client, uint32_t num)
{
  if (!&client) return;
  client.desktop = num;
  set_desktop(current_desktop);
  setprop<long>(client.child, "_NET_WM_DESKTOP", num);
}

const char *getenv (const char *name, const char *def)
{
  const char *env = getenv(name);
  return env ? env : def;
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
  } else if (match_key(event, "M-Return")) {
    spawn(getenv("TERMINAL", "gnome-terminal"));
  } else if (match_key(event, "M-c")) {
    XDestroyClient(client.child);
  } else if (match_key(event, "M-q")) {
    exit(0);
    execlp(command, command, 0);
  } else if (match_key(event, "M-f")) {
    find_screen(event.x_root, event.y_root);
  }
}

int error (Display *dpy, XErrorEvent *error)
{
  char buff[80];
  XGetErrorText(dpy, error->error_code, buff, 80);
  fprintf(stderr, "%s\n", buff);
  return 0;
}

int main (int argc, const char *argv[])
{
  command = argv[0];
  if (!(dpy = XOpenDisplay(0)))
    err(1, "failed to start");
  screens = XineramaQueryScreens(dpy, &screen_count);
  spawn("xmobar -b");
  XSynchronize(dpy, True);
  current_desktop = 1;
  active_frame_pixel   = XMakeColor(dpy, "rgb:f/0/0");
  frame_text_pixel     = XMakeColor(dpy, "rgb:f/f/f");
  inactive_frame_pixel = XMakeColor(dpy, "rgb:8/8/8");
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
      update_name(client);
      move_resize(client, attr.x - 5, attr.y - HeadlineHeight - 1, attr.width, attr.height);
      auto num = getprop<long>(client.child, "_NET_WM_DESKTOP", -1);
      XSetWindowBorderWidth(dpy, client.child, 0);
      XMapWindow(dpy, client.child);
      XMapWindow(dpy, client.frame);
      set_desktop(client, num > -1 ? num : client.desktop);
      printf("Window %s is on %li\n", client.title.c_str(), num);
      XClearWindow(dpy, client.frame);
      XSetWMState(client, 1);
    }
  }
  XSelectInput(dpy, root, KeyPressMask | SubstructureRedirectMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
  XSetWindowBackground(dpy, root, XMakeColor(dpy, "rgb:4/6/8"));
  XClearWindow(dpy, root);
  grab_key(dpy, root, "M-Return");
  grab_key(dpy, root, "M-c");
  grab_key(dpy, root, "M-q");
  grab_key(dpy, root, "M-f");
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
