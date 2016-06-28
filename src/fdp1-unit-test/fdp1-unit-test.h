/*
 * FDP1 Unit Test Utility
 *      Author: Kieran Bingham
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */

#ifndef _FDP1_UNIT_TEST_H_
#define _FDP1_UNIT_TEST_H_

struct fdp1_context {
	char * appname;
	int dev;
	int width;
	int height;
	int num_frames;
	int hex_not_draw;
	int verbose;
};

#endif /* _FDP1_UNIT_TEST_H_ */
