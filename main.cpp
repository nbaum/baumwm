#include <X11/extensions/Xinerama.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <map>
#include <err.h>

#include "config.h"
#include "variables.h"
#include "x11.h"
#include "client.h"
#include "workspaces.h"
#include "util.h"
#include "event.h"

int main (int argc, const char *argv[])
{
  command = argv[0];
  if (!(dpy = XOpenDisplay(0)))
    err(1, "failed to start");
  screens = XineramaQueryScreens(dpy, &screen_count);
  if (!screens) {
    screens = new XineramaScreenInfo[1];
    screens->x_org = 0;
    screens->y_org = 0;
    screens->width = WidthOfScreen(XDefaultScreenOfDisplay(dpy));
    screens->height = HeightOfScreen(XDefaultScreenOfDisplay(dpy));
  }
  XSynchronize(dpy, True);
  current_desktop = 1;
  active_frame_pixel   = XMakeColor(dpy, XGetDefault(dpy, "admiral", "active-color", "rgb:f/4/2"));
  frame_text_pixel     = XMakeColor(dpy, XGetDefault(dpy, "admiral", "text-color", "rgb:f/f/f"));
  inactive_frame_pixel = XMakeColor(dpy, XGetDefault(dpy, "admiral", "inactive-color", "rgb:a/a/a"));
  root = DefaultRootWindow(dpy);
  Window *children, parent;
  unsigned int nchildren;
  current_desktop = getprop<long>(root, "_NET_CURRENT_DESKTOP", -1);
  XQueryTree(dpy, root, &root, &parent, &children, &nchildren);
  for (int j = 0; j < nchildren; j++) {
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, children[j], &attr);
    if (!attr.override_redirect && attr.map_state == IsViewable) {
      auto &client = XFindClient(children[j], True);
      client.mapped = true;
      update_name(client);
      if (!client.undecorated)
        move_resize(client, attr.x - BorderWidth, attr.y - HeadlineHeight - BorderWidth, attr.width, attr.height);
      else
        move_resize(client, attr.x, attr.y, attr.width, attr.height);
      auto num = getprop<long>(client.child, "_NET_WM_DESKTOP", -1);
      XSetWindowBorderWidth(dpy, client.child, 0);
      XMapWindow(dpy, client.child);
      XMapWindow(dpy, client.frame);
      set_desktop(client, num > -1 ? num : client.desktop);
      XClearWindow(dpy, client.frame);
      XSetWMState(client, 1);
      update_name(client);
    }
  }
  XSelectInput(dpy, root, FocusChangeMask | ButtonPressMask | KeyPressMask | SubstructureRedirectMask);
  XDefineCursor(dpy, root, XCreateFontCursor(dpy, XC_left_ptr));
  XSetWindowBackground(dpy, root, XMakeColor(dpy, "rgb:4/6/8"));
  XClearWindow(dpy, root);
  grab_key(dpy, root, "M-Return");
  grab_key(dpy, root, "M-c");
  grab_key(dpy, root, "M-q");
  grab_key(dpy, root, "M-f");
  grab_key(dpy, root, "M-m");
  grab_key(dpy, root, "M-u");
  grab_key(dpy, root, "M-r");
  grab_key(dpy, root, "M-Prior");
  grab_key(dpy, root, "M-Next");
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
  grab_key(dpy, root, "M-C-1");
  grab_key(dpy, root, "M-C-2");
  grab_key(dpy, root, "M-C-3");
  grab_key(dpy, root, "M-C-4");
  grab_key(dpy, root, "M-C-5");
  grab_key(dpy, root, "M-C-6");
  grab_key(dpy, root, "M-C-7");
  grab_key(dpy, root, "M-C-8");
  grab_key(dpy, root, "M-C-9");
  grab_key(dpy, root, "M-C-S-1");
  grab_key(dpy, root, "M-C-S-2");
  grab_key(dpy, root, "M-C-S-3");
  grab_key(dpy, root, "M-C-S-4");
  grab_key(dpy, root, "M-C-S-5");
  grab_key(dpy, root, "M-C-S-6");
  grab_key(dpy, root, "M-C-S-7");
  grab_key(dpy, root, "M-C-S-8");
  grab_key(dpy, root, "M-C-S-9");
  grab_key(dpy, root, "M-t");
  grab_key(dpy, root, "M-comma");
  grab_key(dpy, root, "M-period");
  grab_button(dpy, root, "M-1");
  grab_button(dpy, root, "M-2");
  grab_button(dpy, root, "M-S-2");
  grab_button(dpy, root, "M-3");
  grab_button(dpy, root, "M-4");
  grab_button(dpy, root, "M-5");
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
  XSetErrorHandler(error);
  XEventLoop();
  return 0;
}

