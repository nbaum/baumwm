#include <sstream>

struct Geom : XRectangle
{
  bool fullscreen;
};

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
  std::map<int, Geom> geom;
  bool mapped;
};

std::map<Window, XClient *> clients;

XClient *focused;

XFontStruct *fs;

void XDrawFrame (XClient& client, bool active)
{
  unsigned int color;
  int w = client.width + 8, h = client.height + 4 + HeadlineHeight;
  if (client.fullscreen)
    w = client.fullscreen->width, h = client.fullscreen->height;
  if (active)
    color = active_frame_pixel;
  else
    color = inactive_frame_pixel;
  XSetForeground(dpy, client.gc, color);
  XFillRectangle(dpy, client.frame, client.gc, 0, 0, w, h);
  XSetForeground(dpy, client.gc, BlackPixel(dpy, 0));
  XDrawRectangle(dpy, client.frame, client.gc, 3, HeadlineHeight - 1, w - 7, h - HeadlineHeight - 3);
  XSetForeground(dpy, client.gc, frame_text_pixel);
  XDrawString(dpy, client.frame, client.gc, 4, 14, client.title.c_str(), client.title.length());
  char buff[100] = "";
  strcat(buff, "[");
#define FOO(x) \
  if (client.desktop & (1 << (x - 1))) { \
    if (buff[1] != 0) \
      strcat(buff, ", "); \
    strcat(buff, # x); \
    if (current_desktop & (1 << (x - 1)))\
      strcat(buff, "*"); \
  }
  FOO(1);
  FOO(2);
  FOO(3);
  FOO(4);
  FOO(5);
  FOO(6);
  FOO(7);
  FOO(8);
  FOO(9);
  strcat(buff, "]");
  w = XTextWidth(fs, buff, strlen(buff));
  XDrawString(dpy, client.frame, client.gc, client.width - w - 4, 14, buff, strlen(buff));
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
}

void unfocus (XClient& client)
{
  XDrawFrame(client, false);
}

void set_desktop (XClient& client, uint32_t num);

XClient& XFindClient (Window w, bool create)
{
  auto c = clients[w];
  if (!c && create) {
    XWindowAttributes attr;
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, 0, 0);
    c = new XClient { .frame = frame, .child = w, .desktop = getprop<unsigned int>(w, "_NET_WM_DESKTOP", current_desktop) };
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
    if (!fs) fs = XLoadQueryFont(dpy, "-*-profont-*-*-*-*-12-*-*-*-*-*-*-*");
    XSetFont(dpy, c->gc, fs->fid);
    XSelectInput(dpy, frame, ButtonPressMask | ExposureMask | EnterWindowMask | SubstructureNotifyMask | SubstructureRedirectMask);
    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    ProcessHints(*c);
    clients[frame] = clients[w] = c;
    unfocus(*c);
    set_desktop(*c, c->desktop);
  }
  return *c;
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
      XMoveResizeWindow(dpy, client.child, 4, HeadlineHeight, width - 8, height - HeadlineHeight - 4);
      XConfigureEvent ev = {
        .type = ConfigureNotify,
        .event = client.child,
        .window = client.child,
        .x = x + 4,
        .y = y + HeadlineHeight,
        .width = width - 8,
        .height = height - HeadlineHeight - 4
      };
      XSendEvent(dpy, client.child, False, StructureNotifyMask, (XEvent *) &ev);
    }
  } else {
    width = std::min(std::max(std::max(15, client.hints.min_width), width), client.hints.max_width);
    height = std::min(std::max(std::max(15, client.hints.min_height), height), client.hints.max_width);
    if (client.undecorated) {
      XMoveResizeWindow(dpy, client.frame, x, y, width, height);
      XSetWindowBorderWidth(dpy, client.frame, 0);
      XMoveResizeWindow(dpy, client.child, 0, 0, width, height);
    } else {
      XMoveResizeWindow(dpy, client.frame, x, y, width + 8, height + HeadlineHeight + 4);
      XSetWindowBorderWidth(dpy, client.frame, 1);
      XMoveResizeWindow(dpy, client.child, 4, HeadlineHeight, width, height);
    }
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
    client.right = x + width + 10;
    client.bottom = y + height + HeadlineHeight + 6;
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
