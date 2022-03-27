#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "uxn.h"
#include "devices/file.h"

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

#pragma mark - Core

static Device *devsystem, *devconsole;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

static void
inspect(Stack *s, char *name)
{
	Uint8 x, y;
	fprintf(stderr, "\n%s\n", name);
	for(y = 0; y < 0x04; ++y) {
		for(x = 0; x < 0x08; ++x) {
			Uint8 p = y * 0x08 + x;
			fprintf(stderr,
				p == s->ptr ? "[%02x]" : " %02x ",
				s->dat[p]);
		}
		fprintf(stderr, "\n");
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
	case 0xe:
		inspect(&d->u->wst, "Working-stack");
		inspect(&d->u->rst, "Return-stack");
		break;
	}
}

static void
console_deo(Device *d, Uint8 port)
{
	if(port == 0x1)
		d->vector = peek16(d->dat, 0x0);
	if(port > 0x7)
		write(port - 0x7, (char *)&d->dat[port], 1);
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

#pragma mark - Generics

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

static void
run(Uxn *u)
{
	Uint16 vec;
	while((!u->dev[0].dat[0xf]) && (read(0, &devconsole->dat[0x2], 1) > 0)) {
		vec = peek16(devconsole->dat, 0);
		if(!vec) vec = u->ram.ptr; /* continue after last BRK */
		uxn_eval(u, vec);
	}
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb"))) return 0;
	r = fread(u->ram.dat + PAGE_PROGRAM, 1, sizeof(u->ram.dat) - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
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
	/* empty    */ uxn_port(&u, 0x2, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(&u, 0x9, nil_dei, nil_deo);
	/* file     */ uxn_port(&u, 0xa, nil_dei, file_deo);
	/* datetime */ uxn_port(&u, 0xb, datetime_dei, nil_deo);
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
		} else {
			char *p = argv[i];
			while(*p) console_input(&u, *p++);
			console_input(&u, '\n');
		}
	}
	if(!loaded)
		return error("Input", "Missing");

	run(&u);

	return 0;
}
