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

#include "fdp1-unit-test.h"
#include "fdp1-v4l2-helpers.h"

static int fdp1_open_close(struct fdp1_context * fdp1)
{
	int ret;

	start_test(fdp1, "Open Close");

	struct fdp1_v4l2_dev * dev = fdp1_v4l2_open(fdp1);

	if (!dev)
		return TEST_FAIL;

	return fdp1_v4l2_close(dev);
}

static int fdp1_format_tests(struct fdp1_context * fdp1)
{
	int ret;
	int fail = 0;
	enum test_result result;

	start_test(fdp1, "Format Tests");

	struct fdp1_v4l2_dev * dev = fdp1_v4l2_open(fdp1);

	if (!dev)
		return TEST_FAIL;

	/* Expected pass */

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			V4L2_PIX_FMT_YUYV, V4L2_FIELD_INTERLACED);

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			V4L2_PIX_FMT_NV12M, V4L2_FIELD_INTERLACED);

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			V4L2_PIX_FMT_YUV420M, V4L2_FIELD_INTERLACED);

	/* Expected fail */

	result = fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			V4L2_PIX_FMT_ARGB32, V4L2_FIELD_INTERLACED);

	if (result == TEST_PASS)
		fail++;

	fdp1_v4l2_close(dev);

	return fail;
}

int fdp1_open_tests(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	/* Simple open then close test */
	fail += fdp1_open_close(fdp1);
	/* Try out some formats */
	fail += fdp1_format_tests(fdp1);

	return fail;
}
