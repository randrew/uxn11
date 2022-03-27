#ifdef __aarch64__
#include <arm_neon.h>
#include "ppu.h"

void
ppu_redraw(Ppu *p, Uint32 *screen)
{
	uint8x16x4_t pal = vld4q_u8((Uint8*)p->palette);
	Uint8 *fg = p->fg.pixels;
	Uint8 *bg = p->bg.pixels;
	int i;

	p->fg.changed = p->bg.changed = 0;

#ifdef __has_builtin
#if __has_builtin(__builtin_assume)
	__builtin_assume(p->width > 0 && p->height > 0);
#endif
#endif

	for(i = 0; i < (p->width * p->height & ~15); i += 16, fg += 16, bg += 16, screen += 16) {
		uint8x16_t fg8 = vld1q_u8(fg);
		uint8x16_t bg8 = vld1q_u8(bg);
		uint8x16_t px8 = vbslq_u8(vceqzq_u8(fg8), bg8, fg8);
		uint8x16x4_t px = {
			vqtbl1q_u8(pal.val[0], px8),
			vqtbl1q_u8(pal.val[1], px8),
			vqtbl1q_u8(pal.val[2], px8),
			vdupq_n_u8(0xff),
		};
		vst4q_u8((uint8_t*)screen, px);
	}

	for(; i < p->width * p->height; i++)
		screen[i] = p->palette[*fg ? *fg : *bg];
}
#endif
