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

	return 0;
}

