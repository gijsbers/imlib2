/*
 * Property handling
 */
#ifndef PROPS_H
#define PROPS_H

#include <X11/Xatom.h>
#include <X11/Xlib.h>

extern Display     *disp;

void                props_win_set_proto_quit(Window win);

int                 props_clientmsg_check_quit(const XClientMessageEvent * ev);

#endif /* PROPS_H */
