#include <stdio.h>
#include <stdlib.h>

#include "uxn.h"
#include "devices/system.h"
#include "devices/file.h"
#include "devices/datetime.h"

/*
Copyright (c) 2021 Devine Lu Linvega

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

static Device *devfile0;

static int
error(char *msg, const char *err)
{
	fprintf(stderr, "Error %s: %s\n", msg, err);
	return 0;
}

void
system_deo_special(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

static void
console_deo(Device *d, Uint8 port)
{
	FILE *fd = port == 0x8 ? stdout : port == 0x9 ? stderr
												  : 0;
	if(fd) {
		fputc(d->dat[port], fd);
		fflush(fd);
	}
}

static void
file_deo(Device *d, Uint8 port)
{
	file_i_deo(d - devfile0, d, port);
}

static Uint8
file_dei(Device *d, Uint8 port)
{
	return file_i_dei(d - devfile0, d, port);
}

static Uint8
nil_dei(Device *d, Uint8 port)
{
	return d->dat[port];
}

static void
nil_deo(Device *d, Uint8 port)
{
	(void)d;
	(void)port;
}

static int
console_input(Uxn *u, char c)
{
	Device *d = &u->dev[1];
	d->dat[0x2] = c;
	return uxn_eval(u, GETVECTOR(d));
}

static void
run(Uxn *u)
{
	Device *d = &u->dev[0];
	while(!d->dat[0xf]) {
		int c = fgetc(stdin);
		if(c != EOF)
			console_input(u, (Uint8)c);
	}
}

static int
load(Uxn *u, char *filepath)
{
	FILE *f;
	int r;
	if(!(f = fopen(filepath, "rb"))) return 0;
	r = fread(u->ram + PAGE_PROGRAM, 1, 0x10000 - PAGE_PROGRAM, f);
	fclose(f);
	if(r < 1) return 0;
	fprintf(stderr, "Loaded %s\n", filepath);
	return 1;
}

static int
start(Uxn *u)
{
	if(!uxn_boot(u, (Uint8 *)calloc(0x10000, sizeof(Uint8))))
		return error("Boot", "Failed");
	/* system   */ uxn_port(u, 0x0, system_dei, system_deo);
	/* console  */ uxn_port(u, 0x1, nil_dei, console_deo);
	/* empty    */ uxn_port(u, 0x2, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x3, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x4, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x5, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x6, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x7, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x8, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0x9, nil_dei, nil_deo);
	/* file0    */ devfile0 = uxn_port(u, 0xa, file_dei, file_deo);
	/* file1    */ uxn_port(u, 0xb, file_dei, file_deo);
	/* datetime */ uxn_port(u, 0xc, datetime_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xd, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xe, nil_dei, nil_deo);
	/* empty    */ uxn_port(u, 0xf, nil_dei, nil_deo);
	return 1;
}

int
main(int argc, char **argv)
{
	Uxn u;
	int i;
	if(argc < 2)
		return error("Usage", "uxncli game.rom args");
	if(!start(&u))
		return error("Start", "Failed");
	if(!load(&u, argv[1]))
		return error("Load", "Failed");
	if(!uxn_eval(&u, PAGE_PROGRAM))
		return error("Init", "Failed");
	for(i = 2; i < argc; i++) {
		char *p = argv[i];
		while(*p) console_input(&u, *p++);
		console_input(&u, '\n');
	}
	run(&u);
	return 0;
}
