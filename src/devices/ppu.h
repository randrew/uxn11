#include <stdlib.h>

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;

typedef struct Layer {
	Uint8 *pixels;
	Uint8 changed;
} Layer;

typedef struct Ppu {
	Uint32 palette[4];
	Uint16 width, height;
	Layer fg, bg;
} Ppu;

void ppu_palette(Ppu *p, Uint8 *addr);
void ppu_resize(Ppu *p, Uint16 width, Uint16 height);
void ppu_clear(Ppu *p, Layer *layer);
void ppu_redraw(Ppu *p, Uint32 *screen);

void ppu_write(Ppu *p, Layer *layer, Uint16 x, Uint16 y, Uint8 color);
void ppu_blit(Ppu *p, Layer *layer, Uint16 x, Uint16 y, Uint8 *sprite, Uint8 color, Uint8 flipx, Uint8 flipy, Uint8 twobpp);
void ppu_debug(Ppu *p, Uint8 *stack, Uint8 wptr, Uint8 rptr, Uint8 *memory);
