
extern std::map<Window, XClient *> clients;

void set_desktop (uint32_t num)
{
  current_desktop = num;
  for (auto i = clients.begin(); i != clients.end(); ++i) {
    auto &client = *i->second;
    if (&client) {
      if (client.desktop & num && client.mapped) {
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

void flip_desktop (uint32_t num)
{
  set_desktop(current_desktop ^ num);
}

void flip_desktop (XClient& client, uint32_t num)
{
  set_desktop(client, client.desktop ^ num);
}
