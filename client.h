
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
  XineramaScreenInfo *fullscreen;
  bool undecorated;
  uint32_t desktop;
  XSizeHints hints;
  std::map<int, Geom> geom;
};

std::map<Window, XClient *> clients;

XClient *focused;

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
  //XSetForeground(dpy, client.gc, color);
  //XFillRectangle(dpy, client.frame, client.gc, 4, 4, client.width, 15);
  XSetForeground(dpy, client.gc, frame_text_pixel);
  XDrawString(dpy, client.frame, client.gc, 4, 14, client.title.c_str(), client.title.length());
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
  XGrabButton(dpy, 1, AnyModifier, client.frame, False, ButtonPressMask | ButtonReleaseMask, GrabModeSync, GrabModeSync, None, None);
  XDrawFrame(client, false);
}

XClient& XFindClient (Window w, bool create)
{
  auto c = clients[w];
  if (!c && create) {
    XWindowAttributes attr;
    auto frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent,
                               InputOutput, CopyFromParent, 0, 0);
    c = new XClient { .frame = frame, .child = w, .desktop = current_desktop };
    c->width = 500;
    c->height = 300;
    auto s = current_screen();
    if (s) {
      c->width = s->width / 3;
      c->height = s->height / 3;
      c->x = s->width / 2 + s->x_org - c->width / 2;
      c->y = s->height / 2 + s->y_org - c->height / 2;
    }
    XSetWindowBorder(dpy, frame, BlackPixel(dpy, 0));
    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 4, HeadlineHeight);
    int num;
    Atom *atoms = XListProperties(dpy, w, &num);
    c->gc = XCreateGC(dpy, w, 0, 0);
    XSetFont(dpy, c->gc, XLoadFont(dpy, "-*-profont-*-*-*-*-12-*-*-*-*-*-*-*"));
    XSelectInput(dpy, frame, ButtonPressMask | ExposureMask | EnterWindowMask | SubstructureNotifyMask | SubstructureRedirectMask);
    XSelectInput(dpy, w, PropertyChangeMask | StructureNotifyMask);
    ProcessHints(*c);
    clients[frame] = clients[w] = c;
    unfocus(*c);
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
    XRaiseWindow(dpy, client.frame);
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
  }
}

void XSetWMState (XClient& client, int state)
{
  long data[] = { state, None };
  XChangeProperty(dpy, client.child, XInternAtom(dpy, "WM_STATE", False),
                  XInternAtom(dpy, "WM_STATE", False), 32,
                  PropModeReplace, (unsigned char *) data, 2);
}

