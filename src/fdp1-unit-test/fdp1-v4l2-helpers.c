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
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <sys/mman.h>

#include "fdp1-unit-test.h"
#include "fdp1-v4l2-helpers.h"

void start_test(struct fdp1_context * fdp1, char * test)
{
	if (fdp1->verbose)
		puts(test);
}

struct fdp1_v4l2_dev * fdp1_v4l2_open(struct fdp1_context * fdp1)
{
	int ret;
	char devname[] = "/dev/videoNNNNNNN";
	struct fdp1_v4l2_dev * v4l2_dev = malloc(sizeof(struct fdp1_v4l2_dev));

	if (!v4l2_dev)
		return NULL;

	snprintf(devname, sizeof(devname), "/dev/video%d", fdp1->dev);

	kprint(fdp1, 2, "Opening %s\n", devname);

	v4l2_dev->fd = open(devname, O_RDWR | O_NONBLOCK, 0);
	if (v4l2_dev->fd < 0) {
		fprintf(stderr, "%s:%d: failed to open %s", __func__, __LINE__, devname);\
		perror("open");
		free(v4l2_dev);
		return 0;
	}

	ret = ioctl(v4l2_dev->fd, VIDIOC_QUERYCAP, &v4l2_dev->cap);
	if (ret < 0) {
		fprintf(stderr, "%s:%d: failed to query cap %s", __func__, __LINE__, devname);\
		perror("VIDIOC_QUERYCAP");
		fdp1_v4l2_close(v4l2_dev);
		return 0;
	}

	if (!(v4l2_dev->cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
		fprintf(stderr, "Device does not support V4L2_CAP_VIDEO_M2M_MPLANE\n");
		fdp1_v4l2_close(v4l2_dev);
		return 0;
	}

	return v4l2_dev;
}

int fdp1_v4l2_close(struct fdp1_v4l2_dev * v4l2_dev)
{
	if (!v4l2_dev)
		return 0;

	close(v4l2_dev->fd);
	free(v4l2_dev);

	return 0;
}

int fdp1_v4l2_set_fmt(struct fdp1_context * fdp1,
			struct fdp1_v4l2_dev * v4l2_dev,
			uint32_t type,
			uint32_t width,
			uint32_t height,
			uint32_t fourcc,
			uint32_t field)
{
	struct v4l2_format fmt;
	int ret;

	/* The data we send to the device/driver */
	fmt.type			= type;
	fmt.fmt.pix_mp.width		= width;
	fmt.fmt.pix_mp.height		= height;
	fmt.fmt.pix_mp.pixelformat	= fourcc;
	fmt.fmt.pix_mp.field		= field;

	ret = ioctl(v4l2_dev->fd, VIDIOC_S_FMT, &fmt);
	if (ret < 0) {
		fprintf(stderr, "Format not set\n");
		perror("VIDIOC_S_FMT");
		return TEST_FAIL;
	}

	if (fmt.fmt.pix_mp.pixelformat != fourcc) {
		fprintf(stderr, "Format changed\n");
		return TEST_FAIL;
	}

	return TEST_PASS;
}

/*
 * Request some buffers,
 *
 * Returns the number granted
 */
int fdp1_v4l2_request_buffers(struct fdp1_context * fdp1,
			struct fdp1_v4l2_dev * v4l2_dev,
			uint32_t type,
			uint32_t buffers_requested)
{
	struct v4l2_requestbuffers reqbuf;
	int ret;

	memzero(reqbuf);

	reqbuf.count	= buffers_requested;
	reqbuf.type	= type;
	reqbuf.memory	= V4L2_MEMORY_MMAP;
	ret = ioctl(v4l2_dev->fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret < 0) {
		fprintf(stderr, "Request Buffers failed\n");
		perror("VIDIOC_REQBUFS");
		return 0;
	}

	kprint(fdp1, 2, "Got %d buffers\n", reqbuf.count);
	return reqbuf.count;
}

int fdp1_v4l2_query_buffer(struct fdp1_context * fdp1,
		struct fdp1_v4l2_dev * v4l2_dev,
		struct fdp1_v4l2_buffer * fdp1_buf,
		uint32_t type,
		unsigned int idx)
{
	int i;
	int fail = 0;
	int ret;
	struct v4l2_buffer buf;
	buf = (struct v4l2_buffer){ 0, }; /* Zero the buffer */

	buf.type	= type;
	buf.memory	= V4L2_MEMORY_MMAP;
	buf.index	= idx;
	buf.m.planes	= fdp1_buf->planes;
	buf.length	= 1; /* Only one plane ATM */

	ret = ioctl(v4l2_dev->fd, VIDIOC_QUERYBUF, &buf);
	if (ret != 0) {
		perror("ioctl VIDIOC_QUERYBUF");
		return ret;
	}

	kprint(fdp1, 2, "QUERYBUF returned offset: %x : buf.length %d\n",
			buf.m.planes[0].m.mem_offset, buf.m.planes[0].length);

	fdp1_buf->n_planes = buf.length;

	for (i = 0; i < fdp1_buf->n_planes; i++) {
		fdp1_buf->sizes[i] = buf.m.planes[i].length;
		fdp1_buf->mem[i] = mmap(NULL, buf.m.planes[i].length,
			  PROT_READ | PROT_WRITE, MAP_SHARED, v4l2_dev->fd,
			  buf.m.planes[i].m.mem_offset);

		if (fdp1_buf->mem[i] == MAP_FAILED) {
			kprint(fdp1, 1, "Failed to mmap plane %d\n", i);
			perror("mmap");
			fail++;
		}
	}

	/* Should probably unmmap here */

	return fail;
}

/*
 * fdp1_v4l2_allocate_buffers
 *
 * If successful, requests, and queries a set of buffers,
 * and returns them as a 'pool'
 */
struct fdp1_v4l2_buffer_pool *
fdp1_v4l2_allocate_buffers(struct fdp1_context * fdp1,
			struct fdp1_v4l2_dev * v4l2_dev,
			uint32_t type,
			uint32_t buffers_requested)
{
	uint32_t qty;
	uint32_t i;
	uint32_t fail = 0;

	struct fdp1_v4l2_buffer_pool * pool = malloc(sizeof(struct fdp1_v4l2_buffer_pool));
	if (!pool) {
		perror("BufferPool Allocation");
		return NULL;
	}

	if (buffers_requested > MAX_BUFFER_POOL_SIZE)
		buffers_requested = MAX_BUFFER_POOL_SIZE;

	pool->qty = fdp1_v4l2_request_buffers(fdp1, v4l2_dev,
			type, buffers_requested);

	if (pool->qty == 0) {
		kprint(fdp1, 2, "Failed to get any buffers\n");
		free(pool);
		return NULL;
	}

	for (i = 0; i < pool->qty; ++i) {
		fail += fdp1_v4l2_query_buffer(fdp1, v4l2_dev,
				&pool->buffer[i], type, i);
	}

	if (fail) {
		kprint(fdp1, 2, "Failed to query buffers\n");
		free(pool);
		return NULL;
	}

	return pool;
}

/* Releases all mmapped memory and free's the pool */
void fdp1_v4l2_free_buffers(struct fdp1_v4l2_buffer_pool * pool)
{
	unsigned int i, k;

	if (!pool)
		return;

	for (i = 0; i < pool->qty; ++i) {
		struct fdp1_v4l2_buffer * buf = &pool->buffer[i];
		for (k = 0; k < buf->n_planes; ++k)
			munmap(buf->mem[k], buf->sizes[k]);;
	}

	free(pool);
}

static int fdp1_set_input_output_formats(struct fdp1_context * fdp1,
		struct fdp1_v4l2_dev * dev,
		uint32_t out_fourcc,
		uint32_t cap_fourcc)
{
	uint32_t fail = 0;

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			fdp1->width, fdp1->height,
			out_fourcc, V4L2_FIELD_NONE);

	fail += fdp1_v4l2_set_fmt(fdp1, dev, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			fdp1->width, fdp1->height,
			cap_fourcc, V4L2_FIELD_NONE);

	kprint(fdp1, 2, "Format set to 0x%x:0x%x for test (fail=%d)\n",
			out_fourcc, cap_fourcc, fail);

	return fail;
}

struct fdp1_m2m *
fdp1_create_m2m(struct fdp1_context * fdp1,
		uint32_t out_fourcc,
		uint32_t cap_fourcc)
{
	int ret;
	int fail = 0;
	enum test_result result;
	struct fdp1_v4l2_buffer_pool * src_bufs;
	struct fdp1_v4l2_buffer_pool * dst_bufs;

	kprint(fdp1, 2, "0x%x 0x%x\n", out_fourcc, cap_fourcc);

	struct fdp1_m2m * m2m = calloc(1, sizeof(struct fdp1_m2m));
	if (!m2m) {
		perror("M2M Ctx Allocation");
		return NULL;
	}

	m2m->dev = fdp1_v4l2_open(fdp1);

	if (!m2m->dev) {
		free(m2m);
		return NULL;
	}

	fail = fdp1_set_input_output_formats(fdp1, m2m->dev,
			out_fourcc, cap_fourcc);

	if (fail) {
		/* We need to know our starting condition for the rest of the tests here */
		kprint(fdp1, 0, "Failed to set formats for M2M\n");
		fdp1_free_m2m(m2m);
		return NULL;
	}

	m2m->src_bufs = fdp1_v4l2_allocate_buffers(fdp1, m2m->dev,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, 4);
	if (!m2m->src_bufs) {
		kprint(fdp1, 0, "Failed to create a src_buf pool\n");
		fail++;
	}

	m2m->dst_bufs = fdp1_v4l2_allocate_buffers(fdp1, m2m->dev,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, 4);
	if (!m2m->dst_bufs) {
		kprint(fdp1, 0, "Failed to create a dst_buf pool\n");
		fail++;
	}

	if (fail) {
		fdp1_free_m2m(m2m);
		return NULL;
	}

	return m2m;
}

void fdp1_free_m2m(struct fdp1_m2m * m2m)
{
	if (!m2m)
		return;

	/* Clean Up */
	fdp1_v4l2_free_buffers(m2m->src_bufs);
	fdp1_v4l2_free_buffers(m2m->dst_bufs);
	fdp1_v4l2_close(m2m->dev);
	free(m2m);
}
