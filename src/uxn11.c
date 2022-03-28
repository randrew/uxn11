#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <poll.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/screen.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/file.h"
#include "devices/datetime.h"

static XImage *ximage;
static Display *display;
static Visual *visual;
static Window window;

static Device *devscreen, *devctrl, *devmouse, *devfile0;

#define WIDTH 64 * 8
#define HEIGHT 40 * 8

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

void
system_deo_special(Device *d, Uint8 port)
{
	if(port > 0x7 && port < 0xe)
		screen_palette(&uxn_screen, &d->dat[0x8]);
}

static void
file_deo(Device *d, Uint8 port)
{
	file_i_deo(d - devfile0, d, port);
}

static Uint8
file_dei(Device *d, Uint8 port)
{
	return file_i_dei(d - devfile0, d, port);
}

static int
console_input(Uxn *u, char c)
{
	Device *d = &u->dev[1];
	d->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(d));
}

static void
console_deo(Device *d, Uint8 port)
{
	FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
												  : 0;
	if(fd) {
		fputc(d->dat[port], fd);
		fflush(fd);
	}
}

static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb"))) return 0;
	r = fread(u->ram + PAGE_PROGRAM, 1, 0x10000 - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

static void
redraw(void)
{
	screen_redraw(&uxn_screen, uxn_screen.pixels);
	XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, uxn_screen.width, uxn_screen.height);
}

static void
hide_cursor(void)
{
	Cursor blank;
	Pixmap bitmap;
	XColor black;
	static char empty[] = {0, 0, 0, 0, 0, 0, 0, 0};
	black.red = black.green = black.blue = 0;
	bitmap = XCreateBitmapFromData(display, window, empty, 8, 8);
	blank = XCreatePixmapCursor(display, bitmap, bitmap, &black, &black, 0, 0);
	XDefineCursor(display, window, blank);
	XFreeCursor(display, blank);
	XFreePixmap(display, bitmap);
}

/* /usr/include/X11/keysymdef.h */

#define XK_Escape 0xff1b
#define XK_Left 0xff51
#define XK_Up 0xff52
#define XK_Right 0xff53
#define XK_Down 0xff54
#define XK_Home 0xff50
#define XK_Shift 0xffe1
#define XK_Control 0xffe3
#define XK_Alt 0xffe9

static Uint8
get_button(KeySym sym)
{
	switch(sym) {
	case XK_Up: return 0x10;
	case XK_Down: return 0x20;
	case XK_Left: return 0x40;
	case XK_Right: return 0x80;
	case XK_Control: return 0x01;
	case XK_Alt: return 0x02;
	case XK_Shift: return 0x04;
	case XK_Home: return 0x08;
	}
	return 0x00;
}

static void
processEvent(void)
{
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type) {
	case Expose:
		redraw();
		break;
	case ClientMessage: {
		XDestroyImage(ximage);
		XDestroyWindow(display, window);
		XCloseDisplay(display);
		exit(0);
	} break;
	case KeyPress: {
		XKeyPressedEvent *e = (XKeyPressedEvent *)&ev;
		KeySym sym;
		char buf[7];
		XLookupString(e, buf, 7, &sym, 0);
		controller_down(devctrl, get_button(sym));
		controller_key(devctrl, sym < 0x80 ? sym : buf[0]);
	} break;
	case KeyRelease: {
		XKeyPressedEvent *e = (XKeyPressedEvent *)&ev;
		KeySym sym;
		char buf[7];
		XLookupString(e, buf, 7, &sym, 0);
		controller_up(devctrl, get_button(sym));
	} break;
	case ButtonPress: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_down(devmouse, 0x1 << (e->button - 1));
	} break;
	case ButtonRelease: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_up(devmouse, 0x1 << (e->button - 1));
	} break;
	case MotionNotify: {
		XMotionEvent *e = (XMotionEvent *)&ev;
		mouse_pos(devmouse, e->x, e->y);
	} break;
	}
}

static int
start(Uxn *u, char *rom)
{
	if(!uxn_boot(u, (Uint8 *)calloc(0x10000, sizeof(Uint8))))
		return error("Boot", "Failed");
	if(!load(u, rom))
		return error("Load", "Failed");
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* mouse    */ devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ devfile0 = uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* empty    */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
	screen_resize(&uxn_screen, WIDTH, HEIGHT);
	if(!uxn_eval(u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");
	return 1;
}

static int
init(void)
{
	Atom wmDelete;
	display = XOpenDisplay(NULL);
	visual = DefaultVisual(display, 0);
	window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width, uxn_screen.height, 1, 0, 0);
	if(visual->class != TrueColor)
		return error("Init", "True-color visual failed");
	XSelectInput(display, window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(display, window, &wmDelete, 1);
	XMapWindow(display, window);
	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width, uxn_screen.height, 32, 0);
	hide_cursor();
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u;
	int i;
	char expirations[8];
	struct pollfd fds[2];
	static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};
	if(argc < 2)
		return error("Usage", "uxncli game.rom args");
	if(!start(&u, argv[1]))
		return error("Start", "Failed");
	if(!init())
		return error("Init", "Failed");
	/* console vector */
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++);
		console_input(&u, '\n');
	}
	fds[0].fd = XConnectionNumber(display);
	fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);
	fds[0].events = fds[1].events = POLLIN;
	/* main loop */
	while(1) {
		if(poll(fds, 2, 1000) <= 0)
			continue;
		while(XPending(display))
			processEvent();
		if(poll(&fds[1], 1, 0)) {
			read(fds[1].fd, expirations, 8);    /* Indicate we handled the timer */
			uxn_eval(&u, GETVECTOR(devscreen)); /* Call the vector once, even if the timer fired multiple times */
		}
		if(uxn_screen.fg.changed || uxn_screen.bg.changed)
			redraw();
	}
	XDestroyImage(ximage);
	return 0;
}
