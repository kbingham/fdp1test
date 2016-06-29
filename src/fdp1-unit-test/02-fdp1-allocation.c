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


static int fdp1_set_input_output_format(struct fdp1_context * fdp1, struct fdp1_v4l2_dev * dev, uint32_t fourcc)
{
	uint32_t fail = 0;

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			fourcc, V4L2_FIELD_NONE);

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			fdp1->width, fdp1->height,
			fourcc, V4L2_FIELD_NONE);

	kprint(fdp1, 2, "Format set to 0x%x for test (fail=%d)\n", fourcc, fail);

	return fail;
}

static int fdp1_pool_allocation_test(struct fdp1_context * fdp1)
{
	int ret;
	int fail = 0;
	enum test_result result;
	struct fdp1_v4l2_buffer_pool * src_bufs;
	struct fdp1_v4l2_buffer_pool * dst_bufs;

	start_test(fdp1, "Buffer Allocation Tests");

	struct fdp1_v4l2_dev * dev = fdp1_v4l2_open(fdp1);

	if (!dev)
		return TEST_FAIL;

	/* Test Setup */

	result = fdp1_set_input_output_format(fdp1, dev, V4L2_PIX_FMT_YUYV);

	if (result == TEST_FAIL) {
		/* We need to know our starting condition for the rest of the tests here */
		kprint(fdp1, 0, "Failed to establish test starting criteria\n");
		fdp1_v4l2_close(dev);
		return result;
	}

	src_bufs = fdp1_v4l2_allocate_buffers(fdp1, dev,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_FIELD_NONE, 4);
	if (!src_bufs) {
		kprint(fdp1, 0, "Failed to create a src_buf pool\n");
		fail++;
	}

	dst_bufs = fdp1_v4l2_allocate_buffers(fdp1, dev,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_FIELD_NONE, 4);
	if (!dst_bufs) {
		kprint(fdp1, 0, "Failed to create a dst_buf pool\n");
		fail++;
	}

	/* Clean Up */
	fdp1_v4l2_free_buffers(src_bufs);
	fdp1_v4l2_free_buffers(dst_bufs);
	fdp1_v4l2_close(dev);

	return fail;
}

int fdp1_allocation_tests(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	fail += fdp1_pool_allocation_test(fdp1);

	return fail;
}
