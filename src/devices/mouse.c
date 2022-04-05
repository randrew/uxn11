#include "mouse.h"

/*
Copyright (c) 2021 Devine Lu Linvega
Copyright (c) 2021 Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

void
mouse_down(Uxn* u, Uint8* dat, Uint8 mask)
{
	dat[6] |= mask;
	uxn_eval(u, NEWGETVECTOR(dat));
}

void
mouse_up(Uxn* u, Uint8* dat, Uint8 mask)
{
	dat[6] &= (~mask);
	uxn_eval(u, NEWGETVECTOR(dat));
}

void
mouse_pos(Uxn* u, Uint8* dat, Uint16 x, Uint16 y)
{
	NEWDEVPOKE16(dat, 0x2, x);
	NEWDEVPOKE16(dat, 0x4, y);
	uxn_eval(u, NEWGETVECTOR(dat));
}

void
mouse_scroll(Uxn* u, Uint8* dat, Uint16 x, Uint16 y)
{
	NEWDEVPOKE16(dat, 0xa, x);
	NEWDEVPOKE16(dat, 0xc, -y);
	uxn_eval(u, NEWGETVECTOR(dat));
	NEWDEVPOKE16(dat, 0xa, 0);
	NEWDEVPOKE16(dat, 0xc, 0);
}
