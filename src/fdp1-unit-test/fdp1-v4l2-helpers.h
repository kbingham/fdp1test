/*
 * FDP1 Unit Test Utility
 *      Author: Kieran Bingham
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */


#include <linux/videodev2.h>

#ifndef _FDP1_V4L2_HELPERS_H_
#define _FDP1_V4L2_HELPERS_H_

struct fdp1_v4l2_dev {
	int fd;

	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_control ctrl;
};

void start_test(struct fdp1_context * fdp1, char * test);
struct fdp1_v4l2_dev * fdp1_v4l2_open(struct fdp1_context * fdp1);
int fdp1_v4l2_close(struct fdp1_v4l2_dev * dev);

#endif /* _FDP1_V4L2_HELPERS_H_ */
