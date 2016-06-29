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
#include <sys/ioctl.h>

#include "fdp1-unit-test.h"
#include "fdp1-v4l2-helpers.h"
#include "fdp1-buffer.h"


static int fdp1_test_m2m_object(struct fdp1_context * fdp1)
{
	struct fdp1_m2m * m2m;
	int fail = 0;

	start_test(fdp1, "M2M Object test");

	m2m = fdp1_create_m2m(fdp1, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_YUYV);

	if (!m2m) {
		kprint(fdp1, 0, "Failed to create an M2M object\n");
		return TEST_FAIL;
	}

	fdp1_free_m2m(m2m);

	return fail;
}

static int fdp1_stream_on(struct fdp1_context * fdp1)
{
	struct fdp1_m2m * m2m;
	int fail = 0;
	int ret;
	int i;

	start_test(fdp1, "Stream Test");

	m2m = fdp1_create_m2m(fdp1, V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_YUYV);

	if (!m2m) {
		kprint(fdp1, 0, "Failed to create an M2M object\n");
		return TEST_FAIL;
	}

	for (i = 0; i < m2m->src_queue.pool->qty; i++) {
		fdp1_fill_buffer(&m2m->src_queue.pool->buffer[i]);
		ret = fdp1_v4l2_buffer_pool_queue(m2m->dev, m2m->src_queue.pool, i);
		kprint(fdp1, 1, "Queued output buffer %d from src_bufs (%d)\n", i, ret);

		if (ret)
			fail++;
	}

	for (i = 0; i < m2m->dst_queue.pool->qty; i++) {
		ret = fdp1_v4l2_buffer_pool_queue(m2m->dev, m2m->dst_queue.pool, i);
		kprint(fdp1, 1, "Queued output buffer %d from dst_bufs (%d)\n", i, ret);

		if (ret)
			fail++;
	}

	if (fdp1_m2m_stream_on(m2m, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
		kprint(fdp1, 1, "Failed to stream on OUTPUT\n");
		fail++;
	}

	if (fdp1_m2m_stream_on(m2m, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
		kprint(fdp1, 1, "Failed to stream on CAPTURE\n");
		fail++;
	}

	/* That's all folks */

	fdp1_free_m2m(m2m);

	return fail;
}

int fdp1_stream_on_tests(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	fail += fdp1_test_m2m_object(fdp1);
	fail += fdp1_stream_on(fdp1);

	return fail;
}
