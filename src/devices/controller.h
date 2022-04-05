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

void controller_down(Uxn *u, Uint8 *dat, Uint8 mask);
void controller_up(Uxn *u, Uint8 *dat, Uint8 mask);
void controller_key(Uxn *u, Uint8 *dat, Uint8 key);
void controller_special(Uxn *u, Uint8 *dat, Uint8 key);
