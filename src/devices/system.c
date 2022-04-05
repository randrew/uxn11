#include <stdio.h>

#include "system.h"

/*
Copyright (c) 2022 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static const char *errors[] = {
	"Working-stack underflow",
	"Return-stack underflow",
	"Working-stack overflow",
	"Return-stack overflow",
	"Working-stack division by zero",
	"Return-stack division by zero"};

static void
system_print(Stack *s, char *name)
{
	Uint8 i;
	fprintf(stderr, "<%s>", name);
	for(i = 0; i < s->ptr; i++)
		fprintf(stderr, " %02x", s->dat[i]);
	if(!i)
		fprintf(stderr, " empty");
	fprintf(stderr, "\n");
}

void
system_inspect(Uxn *u)
{
	system_print(u->wst, "wst");
	system_print(u->rst, "rst");
}

int
uxn_halt(Uxn *u, Uint8 error, Uint16 addr)
{
	system_inspect(u);
	fprintf(stderr, "Halted: %s#%04x, at 0x%04x\n", errors[error], u->ram[addr], addr);
	return 0;
}

/* IO */

void
system_deo(Uxn *u, Uint8 *dat, Uint8 port)
{
	switch(port) {
	case 0x2: u->wst = (Stack*)(u->ram + (dat[port] ? (dat[port] * 0x100) : 0x10000)); break;
	case 0x3: u->rst = (Stack*)(u->ram + (dat[port] ? (dat[port] * 0x100) : 0x10100)); break;
	case 0xe: system_inspect(u); break;
	default: system_deo_special(u, dat, port);
	}
}
