#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64 * 8
#define HEIGHT 40 * 8

#include "uxn.h"
#include "devices/system.h"
#include "devices/screen.h"
#include "devices/controller.h"

static Device *devctrl;

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

/* /usr/include/X11/keysymdef.h */

#define XK_Left 0xff51
#define XK_Up 0xff52
#define XK_Right 0xff53
#define XK_Down 0xff54

#define XK_Home 0xff50
#define XK_Shift 0xffe1
#define XK_Control 0xffe3
#define XK_Alt 0xffe9

void
processEvent(Display *display, Window window, XImage *ximage, int width, int height)
{
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type) {
	case Expose:
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, width, height);
		break;
	case KeyPress: {
		XKeyPressedEvent *e = (XKeyPressedEvent *)&ev;
		if(e->keycode == XKeysymToKeycode(display, XK_Up)) controller_down(devctrl, 0x10);
		if(e->keycode == XKeysymToKeycode(display, XK_Down)) controller_down(devctrl, 0x20);
		if(e->keycode == XKeysymToKeycode(display, XK_Left)) controller_down(devctrl, 0x40);
		if(e->keycode == XKeysymToKeycode(display, XK_Right)) controller_down(devctrl, 0x80);
		if(e->keycode == XKeysymToKeycode(display, XK_Control)) controller_down(devctrl, 0x01);
		if(e->keycode == XKeysymToKeycode(display, XK_Alt)) controller_down(devctrl, 0x02);
		if(e->keycode == XKeysymToKeycode(display, XK_Shift)) controller_down(devctrl, 0x04);
		if(e->keycode == XKeysymToKeycode(display, XK_Home)) controller_down(devctrl, 0x08);
	} break;
	case KeyRelease: {
		XKeyPressedEvent *e = (XKeyPressedEvent *)&ev;
		if(e->keycode == XKeysymToKeycode(display, XK_Up)) controller_up(devctrl, 0x10);
		if(e->keycode == XKeysymToKeycode(display, XK_Down)) controller_up(devctrl, 0x20);
		if(e->keycode == XKeysymToKeycode(display, XK_Left)) controller_up(devctrl, 0x40);
		if(e->keycode == XKeysymToKeycode(display, XK_Right)) controller_up(devctrl, 0x80);
		if(e->keycode == XKeysymToKeycode(display, XK_Control)) controller_up(devctrl, 0x01);
		if(e->keycode == XKeysymToKeycode(display, XK_Alt)) controller_up(devctrl, 0x02);
		if(e->keycode == XKeysymToKeycode(display, XK_Shift)) controller_up(devctrl, 0x04);
		if(e->keycode == XKeysymToKeycode(display, XK_Home)) controller_up(devctrl, 0x08);
	} break;
	case ButtonPress:
		exit(0);
	}
}

static int
start(Uxn *u)
{
	if(!uxn_boot(u, (Uint8 *)calloc(0x10000, sizeof(Uint8))))
		return error("Boot", "Failed");
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ uxn_port(u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file     */ uxn_port(u, 0xa, nil_dei, nil_deo);
	/* datetime */ uxn_port(u, 0xb, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xc, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
	return 1;
}

void
redraw(void)
{
}

int
main(int argc, char **argv)
{
	Uxn u;

	if(argc < 2)
		return error("Usage", "uxncli game.rom args");
	if(!start(&u))
		return error("Start", "Failed");
	if(!load(&u, argv[1]))
		return error("Load", "Failed");

	screen_resize(&uxn_screen, WIDTH, HEIGHT);

	if(!uxn_eval(&u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");

	screen_redraw(&uxn_screen, uxn_screen.pixels);

	XImage *ximage;
	Display *display = XOpenDisplay(NULL);
	Visual *visual = DefaultVisual(display, 0);
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, uxn_screen.width, uxn_screen.height, 1, 0, 0);
	if(visual->class != TrueColor) {
		fprintf(stderr, "Cannot handle non true color visual ...\n");
		exit(1);
	}

	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (char *)uxn_screen.pixels, uxn_screen.width, uxn_screen.height, 32, 0);

	XSelectInput(display, window, ButtonPressMask | ExposureMask | KeyPressMask | KeyReleaseMask);
	XMapWindow(display, window);
	while(1) {
		processEvent(display, window, ximage, uxn_screen.width, uxn_screen.height);

		if(uxn_screen.fg.changed || uxn_screen.bg.changed)
			redraw();
	}
	return 0;
}