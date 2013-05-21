#ifndef util_h
#define util_h

inline unsigned long XMakeColor (Display *dpy, const char *s)
{
  XColor color;
  Colormap cm = DefaultColormap(dpy, 0);
  XParseColor(dpy, cm, s, &color);
  XAllocColor(dpy, cm, &color);
  return color.pixel;
}

inline void parse_modifiers (const char *&spec, int &mask)
{
  mask = 0;
  for (; spec[1] == '-'; spec += 2) {
    switch (spec[0]) {
    case 'S':
      mask |= ShiftMask;
      break;
    case 'C':
      mask |= ControlMask;
      break;
    case 'A':
      mask |= Mod1Mask;
      break;
    case 'M':
      mask |= Mod2Mask;
      break;
    }
  }
}

inline void parse_key (const char *spec, int &sym, int &mask)
{
  parse_modifiers(spec, mask);
  sym = XStringToKeysym(spec);
}

inline void parse_button (const char *spec, int &button, int &mask)
{
  parse_modifiers(spec, mask);
  button = atoi(spec);
}

inline void grab_key (Display *dpy, Window w, const char *spec)
{
  int sym, mask;
  parse_key(spec, sym, mask);
  XGrabKey(dpy, sym, mask, w, True, GrabModeAsync, GrabModeAsync);
}

inline void grab_button (Display *dpy, Window w, const char *spec)
{
  int button, mask;
  parse_button(spec, button, mask);
  XGrabButton(dpy, button, mask, w, True, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
}

inline const char *XEventName (int type)
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

#endif