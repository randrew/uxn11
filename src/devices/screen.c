#include <stdlib.h>

#include "../uxn.h"
#include "screen.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static Uint8 blending[5][16] = {
	{0, 0, 0, 0, 1, 0, 1, 1, 2, 2, 0, 2, 3, 3, 3, 0},
	{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3},
	{1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1},
	{2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2, 2, 3, 1, 2},
	{1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0}};

static void
screen_write(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color)
{
	if(x < p->width && y < p->height) {
		Uint32 i = x + y * p->width;
		if(color != layer->pixels[i]) {
			layer->pixels[i] = color;
			layer->changed = 1;
		}
	}
}

static void
screen_blit(UxnScreen *p, Layer *layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 twobpp)
{
	int v, h, opaque = blending[4][color];
	for(v = 0; v < 8; v++) {
		Uint16 c = sprite[v] | (twobpp ? sprite[v + 8] : 0) << 8;
		for(h = 7; h >= 0; --h, c >>= 1) {
			Uint8 ch = (c & 1) | ((c >> 7) & 2);
			if(opaque || ch)
				screen_write(p,
					layer,
					x + (flipx ? 7 - h : h),
					y + (flipy ? 7 - v : v),
					blending[ch][color]);
		}
	}
}

void
screen_palette(UxnScreen *p, Uint8 *addr)
{
	int i, shift;
	for(i = 0, shift = 4; i < 4; ++i, shift ^= 4) {
		Uint8
			r = (addr[0 + i / 2] >> shift) & 0x0f,
			g = (addr[2 + i / 2] >> shift) & 0x0f,
			b = (addr[4 + i / 2] >> shift) & 0x0f;
		p->palette[i] = 0x0f000000 | r << 16 | g << 8 | b;
		p->palette[i] |= p->palette[i] << 4;
	}
	p->fg.changed = p->bg.changed = 1;
}

void
screen_resize(UxnScreen *p, Uint16 width, Uint16 height)
{
	Uint8
		*bg = realloc(p->bg.pixels, width * height),
		*fg = realloc(p->fg.pixels, width * height);
	Uint32
		*pixels = realloc(p->pixels, width * height * sizeof(Uint32));
	if(bg) p->bg.pixels = bg;
	if(fg) p->fg.pixels = fg;
	if(pixels) p->pixels = pixels;
	if(bg && fg && pixels) {
		p->width = width;
		p->height = height;
		screen_clear(p, &p->bg);
		screen_clear(p, &p->fg);
	}
}

void
screen_clear(UxnScreen *p, Layer *layer)
{
	Uint32 i, size = p->width * p->height;
	for(i = 0; i < size; i++)
		layer->pixels[i] = 0x00;
	layer->changed = 1;
}

void
screen_redraw(UxnScreen *p)
{
	Uint32 *pixels = p->pixels;
	Uint32 i, size = p->width * p->height, palette[16];
	for(i = 0; i < 16; i++)
		palette[i] = p->palette[(i >> 2) ? (i >> 2) : (i & 3)];
	for(i = 0; i < size; i++)
		pixels[i] = palette[p->fg.pixels[i] << 2 | p->bg.pixels[i]];
	p->fg.changed = p->bg.changed = 0;
}

int
clamp(int val, int min, int max)
{
	return (val >= min) ? (val <= max) ? val : max : min;
}

/* IO */

Uint8
screen_dei(UxnScreen *screen, Uint8 *dat, Uint8 port)
{
	switch(port) {
	case 0x2: return screen->width >> 8;
	case 0x3: return screen->width;
	case 0x4: return screen->height >> 8;
	case 0x5: return screen->height;
	default: return dat[port];
	}
}

void
screen_deo(Uxn *u, UxnScreen *screen, Uint8 *dat, Uint8 port)
{
	switch(port) {
	case 0x3:
		if(!FIXED_SIZE) {
			Uint16 w;
			NEWDEVPEEK16(w, dat, 0x2);
			screen_resize(screen, clamp(w, 1, 1024), screen->height);
		}
		break;
	case 0x5:
		if(!FIXED_SIZE) {
			Uint16 h;
			NEWDEVPEEK16(h, dat, 0x4);
			screen_resize(screen, screen->width, clamp(h, 1, 1024));
		}
		break;
	case 0xe: {
		Uint16 x, y;
		Uint8 layer = dat[0xe] & 0x40;
		NEWDEVPEEK16(x, dat, 0x8);
		NEWDEVPEEK16(y, dat, 0xa);
		screen_write(screen, layer ? &screen->fg : &screen->bg, x, y, dat[0xe] & 0x3);
		if(dat[0x6] & 0x01) NEWDEVPOKE16(dat, 0x8, x + 1); /* auto x+1 */
		if(dat[0x6] & 0x02) NEWDEVPOKE16(dat, 0xa, y + 1); /* auto y+1 */
		break;
	}
	case 0xf: {
		Uint16 x, y, dx, dy, addr;
		Uint8 i, n, twobpp = !!(dat[0xf] & 0x80);
		Layer *layer = (dat[0xf] & 0x40) ? &screen->fg : &screen->bg;
		NEWDEVPEEK16(x, dat, 0x8);
		NEWDEVPEEK16(y, dat, 0xa);
		NEWDEVPEEK16(addr, dat, 0xc);
		n = dat[0x6] >> 4;
		dx = (dat[0x6] & 0x01) << 3;
		dy = (dat[0x6] & 0x02) << 2;
		if(addr > 0x10000 - ((n + 1) << (3 + twobpp)))
			return;
		for(i = 0; i <= n; i++) {
			screen_blit(screen, layer, x + dy * i, y + dx * i, &u->ram[addr], dat[0xf] & 0xf, dat[0xf] & 0x10, dat[0xf] & 0x20, twobpp);
			addr += (dat[0x6] & 0x04) << (1 + twobpp);
		}
		NEWDEVPOKE16(dat, 0xc, addr);   /* auto addr+length */
		NEWDEVPOKE16(dat, 0x8, x + dx); /* auto x+8 */
		NEWDEVPOKE16(dat, 0xa, y + dy); /* auto y+8 */
		break;
	}
	}
}
