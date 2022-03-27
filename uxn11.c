#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH 64 * 8
#define HEIGHT 40 * 8
#define FIXED_SIZE 0

#include "uxn.h"

#include "devices/ppu.h"
static Ppu ppu;

static Device *devsystem, *devconsole, *devscreen;
static Uint32 *ppu_screen;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

static int
set_size(Uint16 width, Uint16 height, int is_resize)
{
	ppu_resize(&ppu, width, height);
	if(!(ppu_screen =
			   realloc(ppu_screen, ppu.width * ppu.height * sizeof(Uint32))))
		return error("ppu_screen", "Memory failure");
	memset(ppu_screen, 0, ppu.width * ppu.height * sizeof(Uint32));
	printf("%d x %d(%d)\n", width, height, is_resize);
	return 1;
}

static Uint8
system_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2:
		return d->u->wst.ptr;
	case 0x3:
		return d->u->rst.ptr;
	default:
		return d->dat[port];
	}
}

static void
system_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2:
		d->u->wst.ptr = d->dat[port];
		break;
	case 0x3:
		d->u->rst.ptr = d->dat[port];
		break;
	}
	if(port > 0x7 && port < 0xe)
		ppu_palette(&ppu, &d->dat[0x8]);
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
}

static Uint8
screen_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2:
		return ppu.width >> 8;
	case 0x3:
		return ppu.width;
	case 0x4:
		return ppu.height >> 8;
	case 0x5:
		return ppu.height;
	default:
		return d->dat[port];
	}
}

static void
screen_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1:
		d->vector = peek16(d->dat, 0x0);
		break;
	case 0x5:
		if(!FIXED_SIZE)
			set_size(peek16(d->dat, 0x2), peek16(d->dat, 0x4), 1);
		break;
	case 0xe: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xe] & 0x40;
		ppu_write(&ppu, layer ? &ppu.fg : &ppu.bg, x, y, d->dat[0xe] & 0x3);
		if(d->dat[0x6] & 0x01)
			poke16(d->dat, 0x8, x + 1); /* auto x+1 */
		if(d->dat[0x6] & 0x02)
			poke16(d->dat, 0xa, y + 1); /* auto y+1 */
		break;
	}
	case 0xf: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Layer *layer = (d->dat[0xf] & 0x40) ? &ppu.fg : &ppu.bg;
		Uint8 *addr = &d->mem[peek16(d->dat, 0xc)];
		Uint8 twobpp = !!(d->dat[0xf] & 0x80);
		ppu_blit(&ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20, twobpp);
		if(d->dat[0x6] & 0x04)
			poke16(d->dat, 0xc, peek16(d->dat, 0xc) + 8 + twobpp * 8); /* auto addr+8 / auto addr+16 */
		if(d->dat[0x6] & 0x01)
			poke16(d->dat, 0x8, x + 8); /* auto x+8 */
		if(d->dat[0x6] & 0x02)
			poke16(d->dat, 0xa, y + 8); /* auto y+8 */
		break;
	}
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
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
}

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	return 0;
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb")))
		return 0;
	r = fread(u->ram.dat + PAGE_PROGRAM, 1, sizeof(u->ram.dat) - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1)
		return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

// ---------------------------

void
processEvent(Display *display, Window window, XImage *ximage, int width, int height)
{
	static char *tir = "This is red";
	static char *tig = "This is green";
	static char *tib = "This is blue";
	XEvent ev;
	XNextEvent(display, &ev);
	switch(ev.type) {
	case Expose:
		XPutImage(display, window, DefaultGC(display, 0), ximage, 0, 0, 0, 0, width, height);
		XSetForeground(display, DefaultGC(display, 0), 0x00ff0000); // red
		XDrawString(display, window, DefaultGC(display, 0), 32, 32, tir, strlen(tir));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 32, tir, strlen(tir));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 32 + 256, tir, strlen(tir));
		XDrawString(display, window, DefaultGC(display, 0), 32, 32 + 256, tir, strlen(tir));
		XSetForeground(display, DefaultGC(display, 0), 0x0000ff00); // green
		XDrawString(display, window, DefaultGC(display, 0), 32, 52, tig, strlen(tig));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 52, tig, strlen(tig));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 52 + 256, tig, strlen(tig));
		XDrawString(display, window, DefaultGC(display, 0), 32, 52 + 256, tig, strlen(tig));
		XSetForeground(display, DefaultGC(display, 0), 0x000000ff); // blue
		XDrawString(display, window, DefaultGC(display, 0), 32, 72, tib, strlen(tib));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 72, tib, strlen(tib));
		XDrawString(display, window, DefaultGC(display, 0), 32 + 256, 72 + 256, tib, strlen(tib));
		XDrawString(display, window, DefaultGC(display, 0), 32, 72 + 256, tib, strlen(tib));
		break;
	case ButtonPress:
		exit(0);
	}
}

int
main(int argc, char **argv)
{

	Uxn u;
	int i, loaded = 0;

	if(!uxn_boot(&u))
		return error("Boot", "Failed");

	/* system   */ devsystem = uxn_port(&u, 0x0, system_dei, system_deo);
	/* console  */ devconsole = uxn_port(&u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(&u, 0x2, screen_dei, screen_deo);
	/* empty    */ uxn_port(&u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x9, nil_dei, nil_deo);
	/* file     */ uxn_port(&u, 0xa, nil_dei, nil_deo);
	/* datetime */ uxn_port(&u, 0xb, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xc, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0xf, nil_dei, nil_deo);

	for(i = 1; i < argc; ++i) {
		if(!loaded++) {
			if(!load(&u, argv[i]))
				return error("Load", "Failed");
			if(!uxn_eval(&u, PAGE_PROGRAM))
				return error("Init", "Failed");
		}
	}
	if(!loaded)
		return error("Input", "Missing");

	if(!set_size(WIDTH, HEIGHT, 0))
		return error("Window", "Failed to set window size.");

	if(!uxn_eval(&u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");
	ppu_redraw(&ppu, ppu_screen);

	XImage *ximage;
	Display *display = XOpenDisplay(NULL);
	Visual *visual = DefaultVisual(display, 0);
	Window window = XCreateSimpleWindow(display, RootWindow(display, 0), 0, 0, ppu.width, ppu.height, 1, 0, 0);
	if(visual->class != TrueColor) {
		fprintf(stderr, "Cannot handle non true color visual ...\n");
		exit(1);
	}

	ximage = XCreateImage(display, visual, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0, (Uint8 *)ppu_screen, ppu.width, ppu.height, 32, 0);

	XSelectInput(display, window, ButtonPressMask | ExposureMask);
	XMapWindow(display, window);
	while(1) {
		processEvent(display, window, ximage, ppu.width, ppu.height);
	}
}