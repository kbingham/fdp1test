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
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#ifndef _FDP1_UNIT_TEST_H_
#define _FDP1_UNIT_TEST_H_

enum test_result {
	TEST_PASS = 0,
	TEST_FAIL,
};

struct fdp1_context {
	char * appname;
	int dev;
	int width;
	int height;
	int num_frames;
	int hex_not_draw;
	int verbose;
};

int fdp1_open_tests(struct fdp1_context * fdp1);
int fdp1_allocation_tests(struct fdp1_context * fdp1);
int fdp1_stream_on_tests(struct fdp1_context * fdp1);
int fdp1_progressive(struct fdp1_context * fdp1);

#define memzero(x)\
	memset(&(x), 0, sizeof (x));

/* It's like printk ... but better */
#define kprint(fdp1, level, fmt, args...) \
	if (fdp1->verbose >= level) \
		fprintf(stderr, "%s:%d: " fmt, __FUNCTION__, __LINE__, ##args)

#endif /* _FDP1_UNIT_TEST_H_ */
