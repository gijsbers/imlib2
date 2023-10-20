/*
 * Property handling
 */
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include "props.h"

static Atom         ATOM_WM_DELETE_WINDOW = None;
static Atom         ATOM_WM_PROTOCOLS = None;

void
props_win_set_proto_quit(Window win)
{
   ATOM_WM_PROTOCOLS = XInternAtom(disp, "WM_PROTOCOLS", False);
   ATOM_WM_DELETE_WINDOW = XInternAtom(disp, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(disp, win, &ATOM_WM_DELETE_WINDOW, 1);
}

int
props_clientmsg_check_quit(const XClientMessageEvent * ev)
{
   return ev->message_type == ATOM_WM_PROTOCOLS &&
      (Atom) ev->data.l[0] == ATOM_WM_DELETE_WINDOW;
}
