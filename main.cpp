#include <X11/Xlib.h>
#include <stdlib.h>
#include <unistd.h>

#include <iostream>
#include <map>

#include "display.h"
#include "util.h"

struct Client
{
  Window window, frame;
  int x, y, width, height;
  XButtonEvent start;
  bool fullscreen;
};

std::map<Window, Client *> clients;

Client &client_for_window (Window w)
{
  Client *client = clients[w];
  if (!client) {
    client = clients[w] = new Client { .window = w };
    XSetWindowBorderWidth(dpy, client->window, 0);
    XSetWindowAttributes attr;
    attr.override_redirect = True;
    attr.border_pixel = pixel("rgb:f/0/0");
    attr.background_pixel = pixel("rgb:f/0/0");
    attr.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | FocusChangeMask | ExposureMask | EnterWindowMask;
    client->frame = XCreateWindow(dpy, root, 0, 0, 1, 1, 1, CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &attr);
    XReparentWindow(dpy, client->window, client->frame, 0, 0);
    clients[client->frame] = client;
  }
  return *client;
}

bool handle_configure_request (XConfigureRequestEvent event)
{
  auto &client = client_for_window(event.window);
  client.x = event.x;
  client.y = event.y;
  client.width = event.width;
  client.height = event.height;
  client.fullscreen = false;
  XMoveResizeWindow(dpy, client.window, 0, 0, client.width, client.height);
  XMoveResizeWindow(dpy, client.frame, client.x, client.y, client.width, client.height);
  return true;
}

bool handle_map_request (XMapRequestEvent event)
{
  auto &client = client_for_window(event.window);
  XMapWindow(dpy, client.window);
  XMapWindow(dpy, client.frame);
  return true;
}

bool handle_drag_window_move (XMotionEvent event)
{
  
}

int main()
{
  
  setup();
  
  grab_key(root, "A-f");
  grab_button(root, "A-1");
  grab_button(root, "A-2");
  grab_button(root, "A-3");
  
  if (!fork()) {
    execlp("xterm", "xterm", NULL);
    exit(0);
  }
  
  listen(ConfigureRequest, handle_configure_request);
  listen(MapRequest, handle_map_request);
  listen(ButtonPress, handle_button_press_request);
  
  event_loop();

/*

  for(;;)
  {
 
    case ButtonPress:
      {
        auto &client = *client_for_window(ev.xbutton.subwindow);
        if (ev.xbutton.button == 2) {
          XLowerWindow(dpy, ev.xbutton.subwindow);
        } else {
          client.fullscreen = false;
          XRaiseWindow(dpy, ev.xbutton.subwindow);
          client.start = ev.xbutton;
          client.ox = client.x;
          client.oy = client.oy;
          XGrabPointer(dpy, ev.xbutton.subwindow, True,
                  PointerMotionMask|ButtonReleaseMask, GrabModeAsync,
                  GrabModeAsync, None, None, CurrentTime);
          XWindowAttributes attr;
          XGetWindowAttributes(dpy, ev.xbutton.subwindow, &attr);
          wf = (ev.xbutton.x - attr.x) * 5 / attr.width - 2;
          hf = (ev.xbutton.y - attr.y) * 5 / attr.height - 2;
        }
      }
      break;
    case ButtonRelease:
      XUngrabPointer(dpy, CurrentTime);
      break;
    case MotionNotify:
      {
        while(XCheckTypedEvent(dpy, MotionNotify, &ev));
        auto &client = *client_for_window(ev.xbutton.window);
        int xdiff, ydiff;
        xdiff = (ev.xbutton.x_root - client.start.x_root);
        ydiff = (ev.xbutton.y_root - client.start.y_root);
        client.start.x_root = ev.xbutton.x_root;
        client.start.y_root = ev.xbutton.y_root;
        if (client.start.button == 1) {
          client.x += xdiff;
          client.y += ydiff;
          XMoveResizeWindow(dpy, client.frame, client.x, client.y, client.width, client.height);
        } else if (client.start.button == 3) {
          if (wf < 0) {
            client.x += xdiff;
            client.width -= xdiff;
          } else if (wf > 0) {
            client.width += xdiff;
          }
          if (hf < 0) {
            client.y += ydiff;
            client.height -= ydiff;
          } else if (hf > 0) {
            client.height += ydiff;
          }
          XMoveResizeWindow(dpy, client.frame, client.x, client.y, client.width, client.height);
          XMoveResizeWindow(dpy, client.child, 0, 0, client.width, client.height);
        }        
      }

*/
  
}
