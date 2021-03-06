/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#include "../uxn.h"

#define FIXED_SIZE 0

typedef struct Layer {
	Uint8 *pixels, changed;
} Layer;

typedef struct UxnScreen {
	Uint32 palette[4], *pixels;
	Uint16 width, height;
	Layer fg, bg;
} UxnScreen;

void screen_palette(UxnScreen *p, Uint8 *addr);
void screen_resize(UxnScreen *p, Uint16 width, Uint16 height);
void screen_clear(UxnScreen *p, Layer *layer);
void screen_redraw(UxnScreen *p);

Uint8 screen_dei(UxnScreen *screen, Uint8 *dat, Uint8 port);
void screen_deo(Uxn *u, UxnScreen *screen, Uint8 *dat, Uint8 port);
int clamp(int val, int min, int max);
