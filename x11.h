
template<typename T, typename T2>
void setprop (Window w, const char *name, const T2 &value)
{
  auto atom = XInternAtom(dpy, name, True);
  T temp = value;
  XChangeProperty(dpy, w, atom, atom, 8, PropModeReplace, (unsigned char *) &temp, sizeof(temp));
}

template<typename T, typename T2>
T getprop (Window w, const char *name, const T2 def)
{
  auto atom = XInternAtom(dpy, name, False);
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

bool hasprop (Window w, const char *name)
{
  auto atom = XInternAtom(dpy, name, False);
  void *prop;
  Atom type;
  int format;
  unsigned long items, bytes;
  XGetWindowProperty(dpy, w, atom, 0, 0,
                     False, AnyPropertyType,
                     &type, &format, &items, &bytes,
                     (unsigned char **) &prop);
  return type != None;
}

int proplen (Window w, const char *name)
{
  auto atom = XInternAtom(dpy, name, False);
  const char *prop;
  Atom type;
  int format;
  unsigned long items, bytes;
  XGetWindowProperty(dpy, w, atom, 0, 0,
                     False, AnyPropertyType,
                     &type, &format, &items, &bytes,
                     (unsigned char **) &prop);
  return bytes;
}

template<>
std::string getprop<std::string, const char *> (Window w, const char *name, const char *def)
{
  if (hasprop(w, name)) {
    auto atom = XInternAtom(dpy, name, False);
    const char *prop, *val;
    Atom type;
    int format;
    int len = proplen(w, name);
    unsigned long items, bytes;
    XGetWindowProperty(dpy, w, atom, 0, len,
                       False, AnyPropertyType,
                       &type, &format, &items, &bytes,
                       (unsigned char **) &prop);
    return prop;
  } else {
    return def;
  }
}

Atom atom (const char *name)
{
  return XInternAtom(dpy, name, False);
}

XineramaScreenInfo *screens;

XineramaScreenInfo *find_screen (int x, int y)
{
  for (int i = 0; i < screen_count; ++i) {
    if (x < screens[i].x_org) continue;
    if (y < screens[i].y_org) continue;
    if (x > screens[i].x_org + screens[i].width - 1) continue;
    if (y > screens[i].y_org + screens[i].height - 1) continue;
    return &screens[i]; 
  }
  return &screens[0];
}

XPoint pointer ()
{
  int x, y;
  Window a, b;
  int c, d;
  unsigned int e;
  XQueryPointer(dpy, root, &a, &b, &x, &y, &c, &d, &e);
  return XPoint{(short)x, (short)y};
}

XineramaScreenInfo *current_screen ()
{
  int x, y;
  Window a, b;
  int c, d;
  unsigned int e;
  XPoint p = pointer();
  return find_screen(p.x, p.y);
}
