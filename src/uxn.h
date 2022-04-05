/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef unsigned char Uint8;
typedef signed char Sint8;
typedef unsigned short Uint16;
typedef signed short Sint16;
typedef unsigned int Uint32;

#define PAGE_PROGRAM 0x0100

/* clang-format off */

#define DEVPEEK16(o, x) { (o) = (d->dat[(x)] << 8) + d->dat[(x) + 1]; }
#define DEVPOKE16(x, y) { d->dat[(x)] = (y) >> 8; d->dat[(x) + 1] = (y); }
#define GETVECTOR(d) ((d)->dat[0] << 8 | (d)->dat[1])

#define NEWDEVPEEK16(o, dat, x) ((o) = ((dat)[(x)] << 8) + (dat)[(x) + 1])
#define NEWDEVPOKE16(dat, x, y) ((dat)[(x)] = (y) >> 8, (dat)[(x) + 1] = (y))
#define NEWGETVECTOR(dat) ((dat)[0] << 8 | (dat)[1])

/* clang-format on */

typedef struct {
	Uint8 dat[255],ptr;
} Stack;

typedef struct Device {
	struct Uxn *u;
	Uint8 dat[16];
} Device;

typedef struct Uxn {
	Uint8 *ram;
	Stack *wst, *rst;
	Uint8 (*dei)(struct Uxn *u, Uint8 address);
	void (*deo)(struct Uxn *u, Uint8 address, Uint8 value);
	Device dev[16];
} Uxn;

int uxn_boot(Uxn *u, Uint8 *ram);
int uxn_eval(Uxn *u, Uint16 pc);
int uxn_halt(Uxn *u, Uint8 error, Uint16 addr);
Device *uxn_port(Uxn *u, Uint8 id);
