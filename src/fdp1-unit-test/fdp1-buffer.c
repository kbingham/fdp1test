/*
 * FDP1 Unit Test Utility
 *      Author: Kieran Bingham
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "fdp1-unit-test.h"
#include "fdp1-v4l2-helpers.h"
#include "fdp1-buffer.h"

static char * content_string = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

static char get_content_char(unsigned int n)
{
	int pos = n % 32;
	return content_string[pos];
}

void fdp1_fill_buffer(struct fdp1_v4l2_buffer * buffer)
{
	char * p;
	int i, k;

	for (i=0; i < buffer->n_planes; i++) {
		p = buffer->mem[i];

		for (k = 0; k < buffer->sizes[i]; k++) {
			*p++ = get_content_char(k);
		}
	}
}

void fdp1_clear_buffer(struct fdp1_v4l2_buffer * buffer)
{
	unsigned int i;
	/* White */
	for (i = 0; i < buffer->n_planes; i++)
		memset(buffer->mem[i], 255, buffer->sizes[i]);
}

#if 0
void fdp1_draw_frame(void *mem, unsigned int lines, char * pfx)
{
	unsigned int i, j;
	unsigned int len = lines * width;
	char * pos = (char*)mem;

	printf("%s pos =%x\n", pfx, pos);

	for(i = 0; i < lines; i++)
	{
		for(j = 0; j < width; j++)
		{
			if(isprint(*pos)) /* printable char */
			{
				putchar(*pos);
			}
			else /* other char */
			{
				putchar('.');
			}
			pos++;
		}
		putchar('\n');
	}
}
#endif
