#ifndef display_h
#define display_h

extern Display *dpy;
extern Window root;

void setup ();

unsigned long pixel (const char *spec);
void grab_key (Window w, const char *spec);
void grab_button (Window w, const char *spec);
void event_tick ();
void event_loop (int mask = -1);

void listen (int type, bool (*handler) (XEvent));
void unlisten (int type, bool (*handler) (XEvent));

#define EVENT_TYPE(T) \
inline void listen (int type, bool (*handler) (T)) \
{ \
  listen(type, (bool (*) (XEvent)) handler); \
} \
inline void unlisten (int type, bool (*handler) (T)) \
{ \
  unlisten(type, (bool (*) (XEvent)) handler); \
}

EVENT_TYPE(XKeyEvent)
EVENT_TYPE(XButtonEvent)
EVENT_TYPE(XMotionEvent)
EVENT_TYPE(XCrossingEvent)
EVENT_TYPE(XFocusChangeEvent)
EVENT_TYPE(XKeymapEvent)
EVENT_TYPE(XExposeEvent)
EVENT_TYPE(XGraphicsExposeEvent)
EVENT_TYPE(XNoExposeEvent)
EVENT_TYPE(XVisibilityEvent)
EVENT_TYPE(XCreateWindowEvent)
EVENT_TYPE(XDestroyWindowEvent)
EVENT_TYPE(XUnmapEvent)
EVENT_TYPE(XMapEvent)
EVENT_TYPE(XMapRequestEvent)
EVENT_TYPE(XReparentEvent)
EVENT_TYPE(XConfigureEvent)
EVENT_TYPE(XGravityEvent)
EVENT_TYPE(XResizeRequestEvent)
EVENT_TYPE(XConfigureRequestEvent)
EVENT_TYPE(XCirculateEvent)
EVENT_TYPE(XCirculateRequestEvent)
EVENT_TYPE(XPropertyEvent)
EVENT_TYPE(XSelectionClearEvent)
EVENT_TYPE(XSelectionRequestEvent)
EVENT_TYPE(XSelectionEvent)
EVENT_TYPE(XClientMessageEvent)
EVENT_TYPE(XMappingEvent)
EVENT_TYPE(XErrorEvent)
EVENT_TYPE(XAnyEvent)
EVENT_TYPE(XGenericEvent)
EVENT_TYPE(XGenericEventCookie)

#endif
