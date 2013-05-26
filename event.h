
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
    if (auto fn = event_handlers[event.type])
      fn(event);
  }
}

void button_press (XButtonPressedEvent& event)
{
  Window win = event.subwindow ? event.subwindow : event.window;
  auto &client = XFindClient(win, False);
  if (event.window != root) {
    focus(client, event.window);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
  } else {
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
      XGrabPointer(dpy, win, True,
                  ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                  GrabModeAsync, root, XCreateFontCursor(dpy, cursor),
                  event.time);
    }
  }
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
  //if (event.value_mask & CWWidth) printf("%i", event.width);
  //if (event.value_mask & CWHeight) printf("x%i", event.height);
  //if (event.value_mask & CWX) printf("+%i", event.x);
  //if (event.value_mask & CWY) printf("+%i", event.y);
  //printf("\n");
  if (event.value_mask & CWX) client.x = event.x;
  if (event.value_mask & CWY) client.y = event.y;
  if (event.value_mask & CWWidth) client.width = event.width;
  if (event.value_mask & CWHeight) client.height = event.height;
  move_resize(client, client.x, client.y, client.width, client.height);
  XSetWindowBorderWidth(dpy, client.child, 0);
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
  //focus(client, event.window);
}

void message (XClientMessageEvent& event)
{
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
  auto prop = XGetAtomName(dpy, event.atom);
  if (!&client) return;
  update_name(client);
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
    client.undecorated = false;
    current_screen();
    if (client.fullscreen) {
      client.fullscreen = 0;
    } else {
      auto s = find_screen(event.x_root, event.y_root);
      if (s) client.fullscreen = s;
      client.undecorated = true;
    }
    move_resize(client, client.x, client.y, client.width, client.height);
  } else if (match_key(event, "M-m")) {
    client.undecorated = !client.undecorated;
    if (client.undecorated) {
      move_resize(client, client.x + 5, client.y + HeadlineHeight + 1, client.width, client.height);
    } else {
      move_resize(client, client.x - 5, client.y - HeadlineHeight - 1, client.width, client.height);
    }
  } else if (match_key(event, "M-r")) {
    spawn("/home/nathan/admiral/libexec/run");
  }
}

int error (Display *dpy, XErrorEvent *error)
{
  char buff[80];
  XGetErrorText(dpy, error->error_code, buff, 80);
  fprintf(stderr, "%s\n", buff);
  return 0;
}

