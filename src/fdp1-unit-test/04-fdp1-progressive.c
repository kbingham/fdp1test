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


static int read_progressive_frame(struct fdp1_context * fdp1,
		struct fdp1_m2m * m2m, int last)
{
	int ret;
	int j;
	struct fdp1_v4l2_buffer * buffer;
	int n, bytesused;

	buffer = fdp1_m2m_dequeue_output(m2m);
	if (!buffer) {
		return TEST_FAIL;
	}

	kprint(fdp1, 3, "Dequeued src buffer, index: %d\n", buffer->index);

	/* Enqueue back the buffer (note that the index is preserved) */
	if (!last) {
		fdp1_fill_buffer(buffer);
		if (fdp1_v4l2_queue_buffer(m2m->dev, buffer))
			return TEST_FAIL;

		kprint(fdp1, 3, "Enqueued src buffer, index: %d\n", buffer->index);


#if 0
		if (very verbose)
			draw_frame(buffer, "SrcBuf:");
#endif
	}

	buffer = fdp1_m2m_dequeue_capture(m2m);
	if (!buffer) {
		return TEST_FAIL;
	}

	if (buffer->bytesused == 0) {
		kprint(fdp1, 1, "Capture finished 0 bytes used\n");
	}

	kprint(fdp1, 3, "Dequeued dst buffer, index: %d\n", buffer->index);

#if 0
	if (very verbose)
		draw_frame(buffer, "DstBuf:");

	debug("DstBuf[%d] CRC: 0x%x\n", n,
			crc8(0x00, p_dst_buf[n],
				   dst_buf_size[n]) );

#endif

	/* Enqueue back the buffer */
	if (!last) {
		fdp1_clear_buffer(buffer);

		if (fdp1_v4l2_queue_buffer(m2m->dev, buffer)) {

			kprint(fdp1, 3, "Failed to queue dst buffer, index: %d\n", buffer->index);
			return TEST_FAIL;
		}

		kprint(fdp1, 3, "Enqueued dst buffer, index: %d\n", buffer->index);
	}

	return 0;
}

static int fdp1_run_progressive_frames(struct fdp1_context * fdp1)
{
	struct fdp1_m2m * m2m;
	int fail = 0;
	int ret;
	int i;
	int type;
	int num_frames;

	start_test(fdp1, "Progressive Stream Test");

	m2m = fdp1_create_m2m(fdp1, V4L2_PIX_FMT_YUYV, V4L2_FIELD_NONE,
			V4L2_PIX_FMT_YUYV);

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

	if (fail) {
		/* That's all folks */
		kprint(fdp1, 1, "Failed to establish progressive starting criteria\n");
		fdp1_free_m2m(m2m);
		return fail;
	}

	num_frames = fdp1->num_frames;

	/* Start reading / processing */
	while (num_frames) {
		fd_set read_fds;
		int r;
		int last = 0;

		FD_ZERO(&read_fds);
		FD_SET(m2m->dev->fd, &read_fds);

		kprint(fdp1, 4, "Before select\n");
		r = select(m2m->dev->fd + 1, &read_fds, NULL, NULL, 0);
		if (r < 0) {
			kprint(fdp1, 1, "Select Failed\n");
			perror("select");
			break;
		}

		kprint(fdp1, 4, "After select\n");

		if (num_frames == 1)
			last = 1;

		if (read_progressive_frame(fdp1, m2m, last)) {
			kprint(fdp1, 1, "read_progressive_frame frame failed\n");
			fail++;
			break;
		}

		--num_frames;

		kprint(fdp1, 4, "FRAMES LEFT: %d\n", num_frames);
	}

	fdp1_free_m2m(m2m);

	return fail;
}

int fdp1_progressive(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	fail += fdp1_run_progressive_frames(fdp1);

	return fail;
}
