#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "uxn.h"

#pragma GCC diagnostic push
#pragma clang diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#include <SDL.h>
#include "devices/ppu.h"
#include "devices/apu.h"
#include "devices/file.h"
#pragma GCC diagnostic pop
#pragma clang diagnostic pop

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#define WIDTH 64 * 8
#define HEIGHT 40 * 8
#define PAD 4
#define FIXED_SIZE 0
#define POLYPHONY 4
#define BENCH 0

static SDL_Window *gWindow;
static SDL_Texture *gTexture;
static SDL_Renderer *gRenderer;
static SDL_AudioDeviceID audio_id;
static SDL_Rect gRect;
/* devices */
static Ppu ppu;
static Apu apu[POLYPHONY];
static Device *devsystem, *devscreen, *devmouse, *devctrl, *devaudio0, *devconsole;
static Uint8 zoom = 1;
static Uint32 *ppu_screen, stdin_event, audio0_event;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "%s: %s\n", msg, err);
	return 0;
}

#pragma mark - Generics

static void
audio_callback(void *u, Uint8 *stream, int len)
{
	int i, running = 0;
	Sint16 *samples = (Sint16 *)stream;
	SDL_memset(stream, 0, len);
	for(i = 0; i < POLYPHONY; ++i)
		running += apu_render(&apu[i], samples, samples + len / 2);
	if(!running)
		SDL_PauseAudioDevice(audio_id, 1);
	(void)u;
}

void
apu_finished_handler(Apu *c)
{
	SDL_Event event;
	event.type = audio0_event + (c - apu);
	SDL_PushEvent(&event);
}

static int
stdin_handler(void *p)
{
	SDL_Event event;
	event.type = stdin_event;
	while(read(0, &event.cbutton.button, 1) > 0)
		SDL_PushEvent(&event);
	return 0;
	(void)p;
}

static void
set_window_size(SDL_Window *window, int w, int h)
{
	SDL_Point win, win_old;
	SDL_GetWindowPosition(window, &win.x, &win.y);
	SDL_GetWindowSize(window, &win_old.x, &win_old.y);
	SDL_SetWindowPosition(window, (win.x + win_old.x / 2) - w / 2, (win.y + win_old.y / 2) - h / 2);
	SDL_SetWindowSize(window, w, h);
}

static void
set_zoom(Uint8 scale)
{
	zoom = SDL_clamp(scale, 1, 3);
	set_window_size(gWindow, (ppu.width + PAD * 2) * zoom, (ppu.height + PAD * 2) * zoom);
}

static int
set_size(Uint16 width, Uint16 height, int is_resize)
{
	ppu_resize(&ppu, width, height);
	gRect.x = PAD;
	gRect.y = PAD;
	gRect.w = ppu.width;
	gRect.h = ppu.height;
	if(!(ppu_screen = realloc(ppu_screen, ppu.width * ppu.height * sizeof(Uint32))))
		return error("ppu_screen", "Memory failure");
	memset(ppu_screen, 0, ppu.width * ppu.height * sizeof(Uint32));
	if(gTexture != NULL) SDL_DestroyTexture(gTexture);
	SDL_RenderSetLogicalSize(gRenderer, ppu.width + PAD * 2, ppu.height + PAD * 2);
	gTexture = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ppu.width + PAD * 2, ppu.height + PAD * 2);
	if(gTexture == NULL || SDL_SetTextureBlendMode(gTexture, SDL_BLENDMODE_NONE))
		return error("gTexture", SDL_GetError());
	if(SDL_UpdateTexture(gTexture, NULL, ppu_screen, sizeof(Uint32)) != 0)
		return error("SDL_UpdateTexture", SDL_GetError());
	if(is_resize)
		set_window_size(gWindow, (ppu.width + PAD * 2) * zoom, (ppu.height + PAD * 2) * zoom);
	return 1;
}

static void
capture_screen(void)
{
	const Uint32 format = SDL_PIXELFORMAT_RGB24;
	time_t t = time(NULL);
	char fname[64];
	int w, h;
	SDL_Surface *surface;
	SDL_GetRendererOutputSize(gRenderer, &w, &h);
	surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, format);
	SDL_RenderReadPixels(gRenderer, NULL, format, surface->pixels, surface->pitch);
	strftime(fname, sizeof(fname), "screenshot-%Y%m%d-%H%M%S.bmp", localtime(&t));
	SDL_SaveBMP(surface, fname);
	SDL_FreeSurface(surface);
	fprintf(stderr, "Saved %s\n", fname);
}

static void
redraw(Uxn *u)
{
	if(devsystem->dat[0xe])
		ppu_debug(&ppu, u->wst.dat, u->wst.ptr, u->rst.ptr, u->ram.dat);
	ppu_redraw(&ppu, ppu_screen);
	if(SDL_UpdateTexture(gTexture, &gRect, ppu_screen, ppu.width * sizeof(Uint32)) != 0)
		error("SDL_UpdateTexture", SDL_GetError());
	SDL_RenderClear(gRenderer);
	SDL_RenderCopy(gRenderer, gTexture, NULL, NULL);
	SDL_RenderPresent(gRenderer);
}

static int
init(void)
{
	SDL_AudioSpec as;
	SDL_zero(as);
	as.freq = SAMPLE_FREQUENCY;
	as.format = AUDIO_S16;
	as.channels = 2;
	as.callback = audio_callback;
	as.samples = 512;
	as.userdata = NULL;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
		error("sdl", SDL_GetError());
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			return error("sdl", SDL_GetError());
	} else {
		audio_id = SDL_OpenAudioDevice(NULL, 0, &as, NULL, 0);
		if(!audio_id)
			error("sdl_audio", SDL_GetError());
	}
	gWindow = SDL_CreateWindow("Uxn", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (WIDTH + PAD * 2) * zoom, (HEIGHT + PAD * 2) * zoom, SDL_WINDOW_SHOWN);
	if(gWindow == NULL)
		return error("sdl_window", SDL_GetError());
	gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
	if(gRenderer == NULL)
		return error("sdl_renderer", SDL_GetError());
	stdin_event = SDL_RegisterEvents(1);
	audio0_event = SDL_RegisterEvents(POLYPHONY);
	SDL_CreateThread(stdin_handler, "stdin", NULL);
	SDL_StartTextInput();
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
	return 1;
}

static void
domouse(SDL_Event *event)
{
	Uint8 flag = 0x00;
	Uint16 x = SDL_clamp(event->motion.x - PAD, 0, ppu.width - 1);
	Uint16 y = SDL_clamp(event->motion.y - PAD, 0, ppu.height - 1);
	if(event->type == SDL_MOUSEWHEEL) {
		devmouse->dat[7] = event->wheel.y;
		return;
	}
	poke16(devmouse->dat, 0x2, x);
	poke16(devmouse->dat, 0x4, y);
	devmouse->dat[7] = 0x00;
	switch(event->button.button) {
	case SDL_BUTTON_LEFT: flag = 0x01; break;
	case SDL_BUTTON_RIGHT: flag = 0x10; break;
	}
	switch(event->type) {
	case SDL_MOUSEBUTTONDOWN:
		devmouse->dat[6] |= flag;
		break;
	case SDL_MOUSEBUTTONUP:
		devmouse->dat[6] &= (~flag);
		break;
	}
}

#pragma mark - Devices

static Uint8
system_dei(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: return d->u->wst.ptr;
	case 0x3: return d->u->rst.ptr;
	default: return d->dat[port];
	}
}

static void
system_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x2: d->u->wst.ptr = d->dat[port]; break;
	case 0x3: d->u->rst.ptr = d->dat[port]; break;
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
	case 0x2: return ppu.width >> 8;
	case 0x3: return ppu.width;
	case 0x4: return ppu.height >> 8;
	case 0x5: return ppu.height;
	default: return d->dat[port];
	}
}

static void
screen_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1: d->vector = peek16(d->dat, 0x0); break;
	case 0x5:
		if(!FIXED_SIZE) set_size(peek16(d->dat, 0x2), peek16(d->dat, 0x4), 1);
		break;
	case 0xe: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Uint8 layer = d->dat[0xe] & 0x40;
		ppu_write(&ppu, layer ? &ppu.fg : &ppu.bg, x, y, d->dat[0xe] & 0x3);
		if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 1); /* auto x+1 */
		if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 1); /* auto y+1 */
		break;
	}
	case 0xf: {
		Uint16 x = peek16(d->dat, 0x8);
		Uint16 y = peek16(d->dat, 0xa);
		Layer *layer = (d->dat[0xf] & 0x40) ? &ppu.fg : &ppu.bg;
		Uint8 *addr = &d->mem[peek16(d->dat, 0xc)];
		Uint8 twobpp = !!(d->dat[0xf] & 0x80);
		ppu_blit(&ppu, layer, x, y, addr, d->dat[0xf] & 0xf, d->dat[0xf] & 0x10, d->dat[0xf] & 0x20, twobpp);
		if(d->dat[0x6] & 0x04) poke16(d->dat, 0xc, peek16(d->dat, 0xc) + 8 + twobpp*8); /* auto addr+8 / auto addr+16 */
		if(d->dat[0x6] & 0x01) poke16(d->dat, 0x8, x + 8); /* auto x+8 */
		if(d->dat[0x6] & 0x02) poke16(d->dat, 0xa, y + 8); /* auto y+8 */
		break;
	}
	}
}

static void
file_deo(Device *d, Uint8 port)
{
	switch(port) {
	case 0x1: d->vector = peek16(d->dat, 0x0); break;
	case 0x9: poke16(d->dat, 0x2, file_init(&d->mem[peek16(d->dat, 0x8)])); break;
	case 0xd: poke16(d->dat, 0x2, file_read(&d->mem[peek16(d->dat, 0xc)], peek16(d->dat, 0xa))); break;
	case 0xf: poke16(d->dat, 0x2, file_write(&d->mem[peek16(d->dat, 0xe)], peek16(d->dat, 0xa), d->dat[0x7])); break;
	case 0x5: poke16(d->dat, 0x2, file_stat(&d->mem[peek16(d->dat, 0x4)], peek16(d->dat, 0xa))); break;
	case 0x6: poke16(d->dat, 0x2, file_delete()); break;
	}
}

static Uint8
audio_dei(Device *d, Uint8 port)
{
	Apu *c = &apu[d - devaudio0];
	if(!audio_id) return d->dat[port];
	switch(port) {
	case 0x4: return apu_get_vu(c);
	case 0x2: poke16(d->dat, 0x2, c->i); /* fall through */
	default: return d->dat[port];
	}
}

static void
audio_deo(Device *d, Uint8 port)
{
	Apu *c = &apu[d - devaudio0];
	if(!audio_id) return;
	if(port == 0xf) {
		SDL_LockAudioDevice(audio_id);
		c->len = peek16(d->dat, 0xa);
		c->addr = &d->mem[peek16(d->dat, 0xc)];
		c->volume[0] = d->dat[0xe] >> 4;
		c->volume[1] = d->dat[0xe] & 0xf;
		c->repeat = !(d->dat[0xf] & 0x80);
		apu_start(c, peek16(d->dat, 0x8), d->dat[0xf] & 0x7f);
		SDL_UnlockAudioDevice(audio_id);
		SDL_PauseAudioDevice(audio_id, 0);
	}
}

static Uint8
datetime_dei(Device *d, Uint8 port)
{
	time_t seconds = time(NULL);
	struct tm zt = {0};
	struct tm *t = localtime(&seconds);
	if(t == NULL)
		t = &zt;
	switch(port) {
	case 0x0: return (t->tm_year + 1900) >> 8;
	case 0x1: return (t->tm_year + 1900);
	case 0x2: return t->tm_mon;
	case 0x3: return t->tm_mday;
	case 0x4: return t->tm_hour;
	case 0x5: return t->tm_min;
	case 0x6: return t->tm_sec;
	case 0x7: return t->tm_wday;
	case 0x8: return t->tm_yday >> 8;
	case 0x9: return t->tm_yday;
	case 0xa: return t->tm_isdst;
	default: return d->dat[port];
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
	if(port == 0x1) d->vector = peek16(d->dat, 0x0);
}

/* Boot */

static int
load(Uxn *u, char *rom)
{
	SDL_RWops *f;
	int r;
	if(!(f = SDL_RWFromFile(rom, "rb"))) return 0;
	r = f->read(f, u->ram.dat + PAGE_PROGRAM, 1, sizeof(u->ram.dat) - PAGE_PROGRAM);
	f->close(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", rom);
	SDL_SetWindowTitle(gWindow, rom);
	return 1;
}

static int
start(Uxn *u, char *rom)
{
	if(!uxn_boot(u))
		return error("Boot", "Failed to start uxn.");
	if(!load(u, rom))
		return error("Boot", "Failed to load rom.");

	/* system   */ devsystem = uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ devconsole = uxn_port(u, 0x1, nil_dei, console_deo);
	/* screen   */ devscreen = uxn_port(u, 0x2, screen_dei, screen_deo);
	/* audio0   */ devaudio0 = uxn_port(u, 0x3, audio_dei, audio_deo);
	/* audio1   */ uxn_port(u, 0x4, audio_dei, audio_deo);
	/* audio2   */ uxn_port(u, 0x5, audio_dei, audio_deo);
	/* audio3   */ uxn_port(u, 0x6, audio_dei, audio_deo);
	/* unused   */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* control  */ devctrl = uxn_port(u, 0x8, nil_dei, nil_deo);
	/* mouse    */ devmouse = uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file     */ uxn_port(u, 0xa, nil_dei, file_deo);
	/* datetime */ uxn_port(u, 0xb, datetime_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xc, nil_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* unused   */ uxn_port(u, 0xf, nil_dei, nil_deo);

	if(!uxn_eval(u, PAGE_PROGRAM))
		return error("Boot", "Failed to start rom.");

	return 1;
}

static void
restart(Uxn *u)
{
	set_size(WIDTH, HEIGHT, 1);
	start(u, "boot.rom");
}

static void
doctrl(Uxn *u, SDL_Event *event, int z)
{
	Uint8 flag = 0x00;
	SDL_Keymod mods = SDL_GetModState();
	devctrl->dat[2] &= 0xf8;
	if(mods & KMOD_CTRL) devctrl->dat[2] |= 0x01;
	if(mods & KMOD_ALT) devctrl->dat[2] |= 0x02;
	if(mods & KMOD_SHIFT) devctrl->dat[2] |= 0x04;
	/* clang-format off */
	switch(event->key.keysym.sym) {
	case SDLK_ESCAPE: flag = 0x08; break;
	case SDLK_UP: flag = 0x10; break;
	case SDLK_DOWN: flag = 0x20; break;
	case SDLK_LEFT: flag = 0x40; break;
	case SDLK_RIGHT: flag = 0x80; break;
	case SDLK_F1: if(z) set_zoom(zoom > 2 ? 1 : zoom + 1); break;
	case SDLK_F2: if(z) devsystem->dat[0xe] = !devsystem->dat[0xe]; ppu_clear(&ppu, &ppu.fg); break;
	case SDLK_F3: if(z) capture_screen(); break;
	case SDLK_AC_BACK:
	case SDLK_F4: if(z) restart(u); break;
	}
	/* clang-format on */
	if(z) {
		devctrl->dat[2] |= flag;
		if(event->key.keysym.sym < 0x20 || event->key.keysym.sym == SDLK_DELETE)
			devctrl->dat[3] = event->key.keysym.sym;
		else if((mods & KMOD_CTRL) && event->key.keysym.sym >= SDLK_a && event->key.keysym.sym <= SDLK_z)
			devctrl->dat[3] = event->key.keysym.sym - (mods & KMOD_SHIFT) * 0x20;
	} else
		devctrl->dat[2] &= ~flag;
}

static const char *errors[] = {"underflow", "overflow", "division by zero"};

int
uxn_halt(Uxn *u, Uint8 error, char *name, int id)
{
	fprintf(stderr, "Halted: %s %s#%04x, at 0x%04x\n", name, errors[error - 1], id, u->ram.ptr);
	return 0;
}

static int
console_input(Uxn *u, char c)
{
	devconsole->dat[0x2] = c;
	return uxn_eval(u, devconsole->vector);
}

static int
run(Uxn *u)
{
	redraw(u);
	while(!devsystem->dat[0xf]) {
		SDL_Event event;
		double elapsed, begin = 0;
		int ksym;
		if(!BENCH)
			begin = SDL_GetPerformanceCounter();
		while(SDL_PollEvent(&event) != 0) {
			switch(event.type) {
			case SDL_DROPFILE:
				set_size(WIDTH, HEIGHT, 0);
				start(u, event.drop.file);
				SDL_free(event.drop.file);
				break;
			case SDL_QUIT:
				return error("Run", "Quit.");
			case SDL_TEXTINPUT:
				devctrl->dat[3] = event.text.text[0]; /* fall-thru */
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				doctrl(u, &event, event.type == SDL_KEYDOWN);
				uxn_eval(u, devctrl->vector);
				devctrl->dat[3] = 0;

				if(event.type == SDL_KEYDOWN) {
					ksym = event.key.keysym.sym;
					if(SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_KEYUP, SDL_KEYUP) == 1 && ksym == event.key.keysym.sym)
						goto breakout;
				}
				break;
			case SDL_MOUSEWHEEL:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEMOTION:
				domouse(&event);
				uxn_eval(u, devmouse->vector);
				break;
			case SDL_WINDOWEVENT:
				if(event.window.event == SDL_WINDOWEVENT_EXPOSED)
					redraw(u);
				break;
			default:
				if(event.type == stdin_event) {
					console_input(u, event.cbutton.button);
				} else if(event.type >= audio0_event && event.type < audio0_event + POLYPHONY)
					uxn_eval(u, peek16((devaudio0 + (event.type - audio0_event))->dat, 0));
			}
		}
	breakout:
		uxn_eval(u, devscreen->vector);
		if(ppu.fg.changed || ppu.bg.changed || devsystem->dat[0xe])
			redraw(u);
		if(!BENCH) {
			elapsed = (SDL_GetPerformanceCounter() - begin) / (double)SDL_GetPerformanceFrequency() * 1000.0f;
			SDL_Delay(SDL_clamp(16.666f - elapsed, 0, 1000));
		}
	}
	return error("Run", "Ended.");
}

int
main(int argc, char **argv)
{
	SDL_DisplayMode DM;
	Uxn u;
	int i, loaded = 0;

	if(!init())
		return error("Init", "Failed to initialize emulator.");
	if(!set_size(WIDTH, HEIGHT, 0))
		return error("Window", "Failed to set window size.");

	/* set default zoom */
	if(SDL_GetCurrentDisplayMode(0, &DM) == 0)
		set_zoom(DM.w / 1280);
	for(i = 1; i < argc; ++i) {
		/* get default zoom from flags */
		if(strcmp(argv[i], "-s") == 0) {
			if(i < argc - 1)
				set_zoom(atoi(argv[++i]));
			else
				return error("Opt", "-s No scale provided.");
		} else if(!loaded++) {
			if(!start(&u, argv[i]))
				return error("Boot", "Failed to boot.");
		} else {
			char *p = argv[i];
			while(*p) console_input(&u, *p++);
			console_input(&u, '\n');
		}
	}
	if(!loaded && !start(&u, "boot.rom"))
		return error("usage", "uxnemu [-s scale] file.rom");
	run(&u);
	SDL_Quit();
	return 0;
}
