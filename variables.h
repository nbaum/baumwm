
int screen_count;

const char *command;

XWindowAttributes attr;
XButtonEvent start;
int hf, vf;
Display *dpy;
Window root, bar;
uint32_t current_desktop;

unsigned int active_frame_pixel;
unsigned int frame_text_pixel;
unsigned int inactive_frame_pixel;

