#include <sstream>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

struct cursor_t {
  int x, y;
  long dist (const cursor_t &c, int scale_x, int scale_y) {
    return (c.x - x) * (c.x - x) * scale_x + (c.y - y) * (c.y - y) * scale_y;
  }
} cursor;

struct XClient
{
  Window frame, child;
  GC gc;
  std::string title;
  union { int x, left; };
  union { int y, top; };
  int width, height;
  int right, bottom;
  XineramaScreenInfo *fullscreen;
  bool undecorated;
  uint32_t desktop;
  XSizeHints hints;
  bool mapped;
  bool shaded;
  cursor_t cursor () {
    return (cursor_t) { x + width / 2, y + height / 2 };
  }
  cursor_t cursor (int dx, int dy) {
    if (dx == -1) return (cursor_t) { x, y + height / 2 };
    if (dx == 1) return (cursor_t) { x + width, y + height / 2 };
    if (dy == -1) return (cursor_t) { x + width / 2, y };
    if (dy == 1) return (cursor_t) { x + width / 2, y + height };
    return cursor();
  }
};

std::map<Window, XClient *> clients;

bool IsClient (Window w)
{
  return clients.find(w) != clients.end();
}

XClient *focused;

XFontStruct *fs;

void XDrawFrame (XClient& client, bool active)
{
  int w = client.width + BorderWidth * 2 - 1,
      h = client.height + BorderWidth * 2 + HeadlineHeight - 1,
      t = BorderWidth + HeadlineHeight - 1,
      l = BorderWidth - 1,
      b = h - l,
      r = w - l;
  int color = 0xCCCCCC, tint = 0x111111;
  double text = 0.5;
  if (active) {
    color = 0x444488;
    tint = 0x333333;
    text = 0.8;
  }
  if (client.undecorated) {
  } else {
    if (client.shaded) {
      h = BorderWidth + HeadlineHeight - 1;
      b = h - l;
    }
    XSetForeground(dpy, client.gc, color);
    XFillRectangle(dpy, client.frame, client.gc, 0, 0, w, h);
    if (!client.shaded) {
      XSetForeground(dpy, client.gc, color - tint);
      XDrawLine(dpy, client.frame, client.gc, l, t, r, t);
      XDrawLine(dpy, client.frame, client.gc, l, t, l, b);
      XSetForeground(dpy, client.gc, color + tint);
      XDrawLine(dpy, client.frame, client.gc, r, t, r, b);
      XDrawLine(dpy, client.frame, client.gc, l, b, r, b);
    }
    XSetForeground(dpy, client.gc, color - tint);
    XDrawLine(dpy, client.frame, client.gc, w, 0, w, h);
    XDrawLine(dpy, client.frame, client.gc, 0, h, w, h);
    XSetForeground(dpy, client.gc, color + tint);
    XDrawLine(dpy, client.frame, client.gc, 0, 0, w, 0);
    XDrawLine(dpy, client.frame, client.gc, 0, 0, 0, h);
    auto cs = cairo_xlib_surface_create(dpy, client.frame, XDefaultVisual(dpy, XDefaultScreen(dpy)), w, h);
    auto c = cairo_create(cs);
    cairo_set_source_rgb(c, text, text, text);
    cairo_move_to(c, BorderWidth * 2, HeadlineHeight - BorderWidth + 1);
    cairo_show_text(c, client.title.c_str());
    std::stringstream ss;
    if ((client.desktop & 0x3) == 0x3) ss << "*";
    ss << "[" << __builtin_ctz(current_desktop) + 1 << "]";
    cairo_text_extents_t te;
    cairo_text_extents(c, ss.str().c_str(), &te);
    cairo_move_to(c, w - te.x_advance - BorderWidth * 2, HeadlineHeight - BorderWidth + 1);
    cairo_show_text(c, ss.str().c_str());
  }
}

void XDeleteClient (Window w)
{
  XClientMessageEvent cme;
  cme.type = ClientMessage;
  cme.window = w;
  cme.message_type = atom("WM_PROTOCOLS");
  cme.format = 32;
  cme.data.l[0] = atom("WM_DELETE_WINDOW");
  cme.data.l[1] = 0;
  XSendEvent(dpy, w, false, 0, (XEvent *) &cme);
}

void XDestroyClient (Window w)
{
  if (IsClient(w)) return;
  auto c = clients[w];
  XDestroyWindow(dpy, c->frame);
  clients.erase(c->frame);
  clients.erase(c->child);
  delete c;
}

struct MotifWmHints {
   unsigned long flags;
   unsigned long functions;
   unsigned long decorations;
   long input_mode;
   unsigned long status;
};

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
  MotifWmHints mh;
  client.undecorated = false;
  if (getstruct(client.child, "_MOTIF_WM_HINTS", 32, mh)) {
    if ((mh.flags & 2) == 2)
      client.undecorated = mh.decorations == 0;
  }
}

void unfocus (XClient& client)
{
  XDrawFrame(client, false);
}

void set_desktop (XClient& client, uint32_t num);

XClient& XFindClient (Window w, bool create, bool focus = false)
{
  if (IsClient(w)) {
    return *clients[w];
  } else if (create) {
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, 0, 0);
    auto c = new XClient { .frame = frame, .child = w, .desktop = getprop<unsigned int>(w, "_NET_WM_DESKTOP", current_desktop) };
    auto s = current_screen();
    c->width = s->width / 3;
    c->height = s->height / 3;
    auto p = pointer();
    c->x = p.x - c->width / 2;
    c->y = p.y - c->height / 2;
    c->right = c->x + c->width;
    c->bottom = c->y + c->height;
    c->mapped = false;
    XSetWindowBorder(dpy, frame, BlackPixel(dpy, 0));
    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 4, HeadlineHeight);
    int num;
    Atom *atoms = XListProperties(dpy, w, &num);
    c->gc = XCreateGC(dpy, w, 0, 0);
    //if (!fs) fs = XLoadQueryFont(dpy, "-*-profont-*-*-*-*-12-*-*-*-*-*-*-*");
    if (!fs) fs = XLoadQueryFont(dpy, "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*");
    XSetFont(dpy, c->gc, fs->fid);
    XSelectInput(dpy, frame, ButtonPressMask | ExposureMask | EnterWindowMask | SubstructureNotifyMask | SubstructureRedirectMask);
    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    c->undecorated = WindowType(w) == atom("_NET_WM_WINDOW_TYPE_DOCK");
    ProcessHints(*c);
    clients[frame] = clients[w] = c;
    unfocus(*c);
    set_desktop(*c, c->desktop);
    return *c;
  } else if (focus) {
    return *focused;
  } else {
    XClient *c = 0;
    return *c;
  }
}

void update_name (XClient &client)
{
  if (hasprop(client.child, "_NET_WM_NAME")) {
    client.title = getprop<std::string>(client.child, "_NET_WM_NAME", "");
  } else if (hasprop(client.child, "WM_NAME")) {
    client.title = getprop<std::string>(client.child, "WM_NAME", "");
  } else {
    client.title = "Untitled Window";
  }
  XDrawFrame(client, &client == focused);
}

void focus (XClient& client, Window window = None, bool lockx = false, bool locky = false)
{
  if (focused)
    unfocus(*focused);
  if (&client) {
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, client.frame, &attr);
    if (attr.map_state != IsViewable) return;
    XUngrabButton(dpy, 1, AnyModifier, client.frame);
    XSetInputFocus(dpy, client.child, RevertToPointerRoot, CurrentTime);
    focused = &client;
    XDrawFrame(client, true);
    if (!lockx)
      cursor.x = client.cursor().x;
    //else
    //  cursor.x = std::max(client.left, std::min(client.right, cursor.x));
    if (!locky)
      cursor.y = client.cursor().y;
    //else
    //  cursor.y = std::max(client.top, std::min(client.bottom, cursor.y));
  } else if (window) {
    XSetInputFocus(dpy, window, RevertToPointerRoot, CurrentTime);
    focused = 0;
  }
}

void move_resize (XClient& client, int x, int y, int width, int height)
{
  if (auto s = client.fullscreen) {
    x = s->x_org;
    y = s->y_org;
    width = s->width;
    height = s->height;
    if (client.undecorated) {
      XMoveResizeWindow(dpy, client.frame, x, y, width, height);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, 0, 0, width, height);
      XConfigureEvent ev = {
        .type = ConfigureNotify,
        .event = client.child,
        .window = client.child,
        .x = x,
        .y = y,
        .width = width,
        .height = height
      };
      XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
    } else {
      XMoveResizeWindow(dpy, client.frame, x, y, width, height);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, BorderWidth, BorderWidth,
                             width + BorderWidth * 2,
                             height + HeadlineHeight + BorderWidth * 2);
      XConfigureEvent ev = {
        .type = ConfigureNotify,
        .event = client.child,
        .window = client.child,
        .x = x + BorderWidth,
        .y = y + BorderWidth + HeadlineHeight,
        .width = width,
        .height = height
      };
      XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
    }
  } else {
    if (client.undecorated) {
      XMoveResizeWindow(dpy, client.frame, x, y, width, height);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, 0, 0, width, height);
    } else if (client.shaded) {
      XMoveResizeWindow(dpy, client.frame, x, y,
                        width + BorderWidth * 2,
                        HeadlineHeight + BorderWidth);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, BorderWidth, HeadlineHeight + BorderWidth,
                        width, height);
    } else {
      XMoveResizeWindow(dpy, client.frame, x, y,
                        width + BorderWidth * 2,
                        height + HeadlineHeight + BorderWidth * 2);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, BorderWidth, BorderWidth + HeadlineHeight,
                        width, height);
    }
    XConfigureEvent ev = {
      .type = ConfigureNotify,
      .event = client.child,
      .window = client.child,
      .x = x + BorderWidth,
      .y = y + BorderWidth + HeadlineHeight,
      .width = width,
      .height = height
    };
    XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
    client.x = x;
    client.y = y;
    client.width = width;
    client.height = height;
    client.right = x + client.width;
    client.bottom = y + client.height;
    setprop(client.child, "_NET_FRAME_EXTENTS", (long[]){BorderWidth, BorderWidth, BorderWidth + HeadlineHeight, BorderWidth});
  }
}

void XSetWMState (XClient& client, int state)
{
  long data[] = { state, None };
  XChangeProperty(dpy, client.child, XInternAtom(dpy, "WM_STATE", False),
                  XInternAtom(dpy, "WM_STATE", False), 32,
                  PropModeReplace, (unsigned char *) data, 2);
}

bool overlap (int start1, int end1, int start2, int end2)
{
  if (start2 >= start1 && start2 <= end1) return true;
  if (end2 >= start1 && end2 <= end1) return true;
  if (start1 >= start2 && start1 <= end2) return true;
  if (end1 >= start2 && end1 <= end2) return true;
  return false;
}

void fill (XClient& client)
{
  int min_x = current_screen()->x_org;
  int min_y = current_screen()->y_org;
  int max_x = current_screen()->width + min_x;
  int max_y = current_screen()->height + min_y;
  for (auto i = clients.begin(); i != clients.end(); ++i) {
    auto &c = *i->second;
    if (!&c)
      continue;
    if (&c == &client || (c.desktop & client.desktop) == 0)
      continue;
    if (overlap(c.left, c.right, client.left, client.right)) {
      if (c.top > client.bottom) max_y = std::min(max_y, c.top);
      if (c.bottom < client.top) min_y = std::max(min_y, c.bottom);
    }
    if (overlap(c.top, c.bottom, client.top, client.bottom)) {
      if (c.left > client.right) max_x = std::min(max_x, c.left);
      if (c.right < client.left) min_x = std::max(min_x, c.right);
    }
  }
  move_resize(client, min_x + 5, min_y + 5, max_x - min_x - 20, max_y - min_y - HeadlineHeight - 16);
}
