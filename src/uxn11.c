#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysymdef.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/screen.h"
#include "devices/controller.h"
#include "devices/mouse.h"
#include "devices/file.h"
#include "devices/datetime.h"

typedef struct Emulator {
	Uxn u;
	XImage *ximage;
	Display *display;
	Visual *visual;
	Window window;
	Device *devscreen, *devctrl, *devmouse;
} Emulator;

#define WIDTH (64 * 8)
#define HEIGHT (40 * 8)

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
uxn11_dei(struct Uxn *u, Uint8 addr)
{
	Uint8 p = addr & 0x0f;
	Device *d = &u->dev[addr >> 4];
	switch(addr & 0xf0) {
	case 0x20: screen_dei(d, p); break;
	case 0xa0: file_dei(d, p); break;
	case 0xb0: file_dei(d, p); break;
	case 0xc0: datetime_dei(d, p); break;
	}
	return d->dat[p];
}

static void
uxn11_deo(Uxn *u, Uint8 addr, Uint8 v)
{
	Uint8 p = addr & 0x0f;
	Device *d = &u->dev[addr >> 4];
	d->dat[p] = v;
	switch(addr & 0xf0) {
	case 0x00: system_deo(d, p); break;
	case 0x10: console_deo(d, p); break;
	case 0x20: screen_deo(d, p); break;
	case 0xa0: file_deo(d, p); break;
	case 0xb0: file_deo(d, p); break;
	}
}

static void
redraw(Emulator *m)
{
	screen_redraw(&uxn_screen, uxn_screen.pixels);
	XPutImage(m->display, m->window, DefaultGC(m->display, 0), m->ximage, 0, 0, 0, 0, uxn_screen.width, uxn_screen.height);
}

static void
hide_cursor(Emulator *m)
{
	Cursor blank;
	Pixmap bitmap;
	XColor black;
	static char empty[] = {0, 0, 0, 0, 0, 0, 0, 0};
	black.red = black.green = black.blue = 0;
	bitmap = XCreateBitmapFromData(m->display, m->window, empty, 8, 8);
	blank = XCreatePixmapCursor(m->display, bitmap, bitmap, &black, &black, 0, 0);
	XDefineCursor(m->display, m->window, blank);
	XFreeCursor(m->display, blank);
	XFreePixmap(m->display, bitmap);
}

static Uint8
get_button(KeySym sym)
{
	switch(sym) {
	case XK_Up: return 0x10;
	case XK_Down: return 0x20;
	case XK_Left: return 0x40;
	case XK_Right: return 0x80;
	case XK_Control_L: return 0x01;
	case XK_Alt_L: return 0x02;
	case XK_Shift_L: return 0x04;
	case XK_Home: return 0x08;
	}
	return 0x00;
}

static void
processEvent(Emulator *m)
{
	XEvent ev;
	XNextEvent(m->display, &ev);
	switch(ev.type) {
	case Expose:
		redraw(m);
		break;
	case ClientMessage: {
		XDestroyImage(m->ximage);
		XDestroyWindow(m->display, m->window);
		XCloseDisplay(m->display);
		exit(0);
	} break;
	case KeyPress: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		controller_down(m->devctrl, get_button(sym));
		controller_key(m->devctrl, sym < 0x80 ? sym : buf[0]);
	} break;
	case KeyRelease: {
		KeySym sym;
		char buf[7];
		XLookupString((XKeyPressedEvent *)&ev, buf, 7, &sym, 0);
		controller_up(m->devctrl, get_button(sym));
	} break;
	case ButtonPress: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_down(m->devmouse, 0x1 << (e->button - 1));
	} break;
	case ButtonRelease: {
		XButtonPressedEvent *e = (XButtonPressedEvent *)&ev;
		mouse_up(m->devmouse, 0x1 << (e->button - 1));
	} break;
	case MotionNotify: {
		XMotionEvent *e = (XMotionEvent *)&ev;
		mouse_pos(m->devmouse, e->x, e->y);
	} break;
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
start(Emulator *m, char *rom)
{
	Uxn *u = &m->u; /* temp hack */
	if(!uxn_boot(&m->u, (Uint8 *)calloc(0x10200, sizeof(Uint8))))
		return error("Boot", "Failed");
	if(!load_rom(&m->u, rom))
		return error("Load", "Failed");
	fprintf(stderr, "Loaded %s\n", rom);
	m->u.dei = uxn11_dei;
	m->u.deo = uxn11_deo;

	/* system   */ uxn_port(u, 0x0, nil_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ m->devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ m->devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* mouse    */ m->devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* reserved */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* reserved */ uxn_port(u, 0xf, nil_dei, nil_deo);

	screen_resize(&uxn_screen, WIDTH, HEIGHT);
	if(!uxn_eval(u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");
	return 1;
}

static int
init(Emulator *m)
{
	Atom wmDelete;
	m->display = XOpenDisplay(NULL);
	m->visual = DefaultVisual(m->display, 0);
	m->window = XCreateSimpleWindow(m->display, RootWindow(m->display, 0), 0, 0, uxn_screen.width, uxn_screen.height, 1, 0, 0);
	if(m->visual->class != TrueColor)
		return error("Init", "True-color m->visual failed");
	XSelectInput(m->display, m->window, ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	wmDelete = XInternAtom(m->display, "WM_DELETE_WINDOW", True);
	XSetWMProtocols(m->display, m->window, &wmDelete, 1);
	XMapWindow(m->display, m->window);
	m->ximage = XCreateImage(m->display, m->visual, DefaultDepth(m->display, DefaultScreen(m->display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width, uxn_screen.height, 32, 0);
	hide_cursor(m);
	return 1;
}

int
main(int argc, char **argv)
{
	Emulator m;
	int i;
	char expirations[8];
	struct pollfd fds[2];
	static const struct itimerspec screen_tspec = {{0, 16666666}, {0, 16666666}};
	memset(&m, 0, sizeof m); /* May not be necessary */
	if(argc < 2)
		return error("Usage", "uxncli game.rom args");
	if(!start(&m, argv[1]))
		return error("Start", "Failed");
	if(!init(&m))
		return error("Init", "Failed");
	/* console vector */
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&m.u, *p++);
		console_input(&m.u, '\n');
	}
	fds[0].fd = XConnectionNumber(m.display);
	fds[1].fd = timerfd_create(CLOCK_MONOTONIC, 0);
	timerfd_settime(fds[1].fd, 0, &screen_tspec, NULL);
	fds[0].events = fds[1].events = POLLIN;
	/* main loop */
	while(1) {
		if(poll(fds, 2, 1000) <= 0)
			continue;
		while(XPending(m.display))
			processEvent(&m);
		if(poll(&fds[1], 1, 0)) {
			read(fds[1].fd, expirations, 8);    /* Indicate we handled the timer */
			uxn_eval(&m.u, GETVECTOR(m.devscreen)); /* Call the vector once, even if the timer fired multiple times */
		}
		if(uxn_screen.fg.changed || uxn_screen.bg.changed)
			redraw(&m);
	}
	XDestroyImage(m.ximage);
	return 0;
}
