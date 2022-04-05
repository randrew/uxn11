/*
Copyright (c) 2022 Devine Lu Linvega, Andrew Alderwick

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE.
*/

typedef struct SystemDevice {
	Device device;
	struct UxnScreen *screen;
} SystemDevice;

void system_inspect(Uxn *u);
void system_deo(Uxn *u, Device *d, Uint8 port);
void system_deo_special(Uxn *u, Device *d, Uint8 port);
