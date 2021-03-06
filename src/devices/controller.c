#include "controller.h"

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
controller_down(Uxn *u, Uint8 *dat, Uint8 mask)
{
	if(mask) {
		dat[2] |= mask;
		uxn_eval(u, GETVECTOR(dat));
	}
}

void
controller_up(Uxn *u, Uint8 *dat, Uint8 mask)
{
	if(mask) {
		dat[2] &= (~mask);
		uxn_eval(u, GETVECTOR(dat));
	}
}

void
controller_key(Uxn *u, Uint8 *dat, Uint8 key)
{
	if(key) {
		dat[3] = key;
		uxn_eval(u, GETVECTOR(dat));
		dat[3] = 0x00;
	}
}

void
controller_special(Uxn *u, Uint8 *dat, Uint8 key)
{
	if(key) {
		dat[4] = key;
		uxn_eval(u, GETVECTOR(dat));
		dat[4] = 0x00;
	}
}
