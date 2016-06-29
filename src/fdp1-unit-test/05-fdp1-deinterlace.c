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


static int dequeue_requeue_output(struct fdp1_context * fdp1,
		struct fdp1_m2m * m2m, int last)
{
	struct fdp1_v4l2_buffer * buffer;

	buffer = fdp1_m2m_dequeue_output(m2m);
	if (!buffer) {
		return TEST_FAIL;
	}

	kprint(fdp1, 3, "Dequeued src buffer, index: %d sequence %d\n",
			buffer->index, m2m->src_queue.sequence_out);

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

	return 0;
}

static int dequeue_requeue_capture(struct fdp1_context * fdp1,
		struct fdp1_m2m * m2m, int last)
{
	struct fdp1_v4l2_buffer * buffer;

	buffer = fdp1_m2m_dequeue_capture(m2m);
	if (!buffer) {
		kprint(fdp1, 1, "Failed to dequeue capture buffer\n");
		return TEST_FAIL;
	}

	if (buffer->bytesused == 0) {
		kprint(fdp1, 1, "Capture finished 0 bytes used\n");
	}

	kprint(fdp1, 3, "Dequeued dst buffer, index: %d sequence %d\n",
			buffer->index, m2m->dst_queue.sequence_out);

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

static int read_3d_deinterlaced_frame(struct fdp1_context * fdp1,
		struct fdp1_m2m * m2m, int first, int last)
{
	/*
	 * FIELD 12      34          56
	 * IN:  [TB]    [TB]        [TB]
	 * OUT:     [F1]    [F2][F3]     [F4][F5]
	 */

	/* Buffers have already been queued before this loop is called */

	/* The first buffer stays in the driver */
	if (!first) {
		if (dequeue_requeue_output(fdp1, m2m, last)) {
			kprint(fdp1, 1, "Failed to DQRQ output buffer\n");
			return TEST_FAIL;
		}

		/* Two output buffers are produced for a deinterlaced frame */
		if (dequeue_requeue_capture(fdp1, m2m, last)) {
			kprint(fdp1, 1, "Failed to DQRQ capture buffer 1\n");
			return TEST_FAIL;
		}
	}

	if (dequeue_requeue_capture(fdp1, m2m, last)) {
		kprint(fdp1, 1, "Failed to DQRQ capture buffer 2\n");
		return TEST_FAIL;
	}

	return 0;
}


static int read_2d_deinterlaced_frame(struct fdp1_context * fdp1,
		struct fdp1_m2m * m2m, int first, int last)
{
	/*
	 * FIELD 12         34          56
	 * IN:  [TB]       [TB]        [TB]
	 * OUT:    [F1][F2]    [F3][F4]    [F5][F6]
	 */

	/* Buffers have already been queued before this loop is called */

	/* Two output buffers are produced for a deinterlaced frame */
	if (dequeue_requeue_capture(fdp1, m2m, last)) {
		kprint(fdp1, 1, "Failed to DQRQ capture buffer 1\n");
		return TEST_FAIL;
	}

	if (dequeue_requeue_capture(fdp1, m2m, last)) {
		kprint(fdp1, 1, "Failed to DQRQ capture buffer 2\n");
		return TEST_FAIL;
	}

	/* The input buffer is free to take *after* the two output buffers
	 * that use it are processed
	 */

	if (dequeue_requeue_output(fdp1, m2m, last)) {
		kprint(fdp1, 1, "Failed to DQRQ output buffer\n");
		return TEST_FAIL;
	}

	return 0;
}


static int fdp1_run_deinterlaced(struct fdp1_context * fdp1,
				 enum fdp1_deint_mode deint_mode)
{
	struct fdp1_m2m * m2m;
	int fail = 0;
	int ret;
	int i;
	int num_frames;
	enum fdp1_deint_mode current_mode;
	unsigned int min_cap_bufs = 0;
	unsigned int min_output_bufs = 0;

	start_test(fdp1, "Deinterlaced Test");

	kprint(fdp1, 1, "Starting Deinterled test in Mode %s\n",
			fdp1_deint_mode_str(deint_mode));

	m2m = fdp1_create_m2m(fdp1, V4L2_PIX_FMT_YUYV, V4L2_FIELD_INTERLACED,
			V4L2_PIX_FMT_YUYV);

	if (!m2m) {
		kprint(fdp1, 0, "Failed to create an M2M object\n");
		return TEST_FAIL;
	}

	fdp1_m2m_get_ctrl(m2m, V4L2_CID_MIN_BUFFERS_FOR_OUTPUT, (int*)&min_output_bufs);
	kprint(fdp1, 1, "+++++++ V4L2_CID_MIN_BUFFERS_FOR_OUTPUT %d\n", min_output_bufs);

	fdp1_m2m_get_ctrl(m2m, V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, (int*)&min_cap_bufs);
	kprint(fdp1, 1, "+++++++ MIN_BUFFERS_FOR_CAPTURE %d\n", min_cap_bufs);

	/* Reset after (known) invalid MIN_BUFFERS_FOR_OUTPUT ctrl */
	errno = 0;

	for (i = 0; i < m2m->src_queue.pool->qty; i++) {
		struct fdp1_v4l2_buffer *buffer = &m2m->src_queue.pool->buffer[i];

		fdp1_fill_buffer(buffer);
		ret = fdp1_v4l2_buffer_pool_queue(m2m->dev, m2m->src_queue.pool, i);
		kprint(fdp1, 1, "Queued output buffer %d from src_bufs (%d)\n", i, ret);

		if (ret)
			fail++;
	}

	kprint(fdp1, 2, "Queued %d source (output) buffers\n", i);

	for (i = 0; i < m2m->dst_queue.pool->qty; i++) {
		ret = fdp1_v4l2_buffer_pool_queue(m2m->dev, m2m->dst_queue.pool, i);
		kprint(fdp1, 1, "Queued output buffer %d from dst_bufs (%d)\n", i, ret);

		if (ret)
			fail++;
	}

	kprint(fdp1, 2, "Queued %d dest (capture) buffers\n", i);

	if (fdp1_m2m_set_ctrl(m2m, V4L2_CID_DEINT_MODE, deint_mode)) {
		kprint(fdp1, 1, "Failed to set DEINT MODE\n");
		fail++;
		fdp1_free_m2m(m2m);
		return fail;
	}

	kprint(fdp1, 1, "Deint mode set to ...\n");

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

	/* Deint mode is only set when stream on is called.
	 * We can only 'verify' after we start streaming...
	 */
	if (fdp1_m2m_get_ctrl(m2m, V4L2_CID_DEINT_MODE, (int*)&current_mode))
	{
		kprint(fdp1, 1, "Failed to get DEINT MODE\n");
		fail++;
		fdp1_free_m2m(m2m);
		return fail;
	}

	if (current_mode != deint_mode) {
		fail++;
		kprint(fdp1, 1, "********* Fail++ Deint mode is not as expected. %d != %d\n",
				current_mode, deint_mode);
		/* Continue in the mode the hardware is configured for */
		deint_mode = current_mode;

		/* We could continue - or we could just halt */
		fdp1_free_m2m(m2m);
		return fail;
	}

	num_frames = fdp1->num_frames;

	/* Start reading / processing */
	while (num_frames) {
		int first = num_frames == fdp1->num_frames;
		int last = num_frames == 1;
		int ret;
		/* Blocking until ready is handled by the dequeue function */

		switch(deint_mode) {
		case FDP1_ADAPT2D3D:
		case FDP1_FIXED3D:
			ret = read_3d_deinterlaced_frame(fdp1, m2m, first, last);
			break;
		case FDP1_FIXED2D:
		case FDP1_PREVFIELD:
		case FDP1_NEXTFIELD:
			ret = read_2d_deinterlaced_frame(fdp1, m2m, first, last);
			break;

		default:
			ret = -1;
			kprint(fdp1, 1, "Unsupported Deinterlace Test Case\n");
			break;
		}

		if (ret) {
			kprint(fdp1, 1, "process frame operation failed\n");
			fail++;
			break;
		}

		--num_frames;

		kprint(fdp1, 4, "FRAMES LEFT: %d\n", num_frames);
	}

	fdp1_free_m2m(m2m);

	return fail;
}

int fdp1_deinterlace(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	fail += fdp1_run_deinterlaced(fdp1, FDP1_ADAPT2D3D);
	fail += fdp1_run_deinterlaced(fdp1, FDP1_FIXED2D);
	fail += fdp1_run_deinterlaced(fdp1, FDP1_FIXED3D);
	fail += fdp1_run_deinterlaced(fdp1, FDP1_PREVFIELD);
	fail += fdp1_run_deinterlaced(fdp1, FDP1_NEXTFIELD);

	return fail;
}
