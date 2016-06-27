/*
 * FDP1 Test Utility
 *      Author: Kieran Bingham
 *
 * Heavily based on process-vmalloc test utility:
 * Pawel Osciak, p.osciak at samsung.com
 * 2009, Samsung Electronics Co., Ltd.
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

#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <sys/mman.h>

#include "crc.h"

#define VIDEO_DEV_NAME	"/dev/video0"
#define NUM_BUFS	4
#define NUM_FRAMES	16

#define perror_exit(cond, func)\
	if (cond) {\
		fprintf(stderr, "%s:%d: ", __func__, __LINE__);\
		perror(func);\
		exit(EXIT_FAILURE);\
	}

#define error_exit(cond, func)\
	if (cond) {\
		fprintf(stderr, "%s:%d: failed\n", func, __LINE__);\
		exit(EXIT_FAILURE);\
	}

#define perror_ret(cond, func)\
	if (cond) {\
		fprintf(stderr, "%s:%d: ", __func__, __LINE__);\
		perror(func);\
		return ret;\
	}

#define memzero(x)\
	memset(&(x), 0, sizeof (x));

#define PROCESS_DEBUG 1
#ifdef PROCESS_DEBUG
#define debug(msg, ...)\
	fprintf(stderr, "%s: " msg, __func__, ##__VA_ARGS__);
#else
#define debug(msg, ...)
#endif

/* Globals to make non-global? */
int width = 128;
int height = 80;
static int vid_fd;
static char *p_src_buf[NUM_BUFS], *p_dst_buf[NUM_BUFS];
static size_t src_buf_size[NUM_BUFS], dst_buf_size[NUM_BUFS];
static uint32_t num_src_bufs = 0, num_dst_bufs = 0;


int queue_buffer(int type, int memory, int index, int len);
int dequeue_output(int *n);
int dequeue_capture(int *n, uint32_t *bytesused);


int curr_buf = 0;
int num_frames = 6;

int hex_not_draw = 0;

#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 8
#endif

void hexdump(void *mem, unsigned int len, char * pfx)
{
        unsigned int i, j;

        for(i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
        {
                /* print offset */
                if(i % HEXDUMP_COLS == 0)
                {
                        printf("%s0x%06x: ", pfx, i);
                }

                /* print hex data */
                if(i < len)
                {
                        printf("%02x ", 0xFF & ((char*)mem)[i]);
                }
                else /* end of block, just aligning for ASCII dump */
                {
                        printf("   ");
                }

                /* print ASCII dump */
                if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
                {
                        for(j = i - (HEXDUMP_COLS - 1); j <= i; j++)
                        {
                                if(j >= len) /* end of block, not really printing */
                                {
                                        putchar(' ');
                                }
                                else if(isprint(((char*)mem)[j])) /* printable char */
                                {
                                        putchar(0xFF & ((char*)mem)[j]);
                                }
                                else /* other char */
                                {
                                        putchar('.');
                                }
                        }
                        putchar('\n');
                }
        }
}

void draw_frame(void *mem, unsigned int lines, char * pfx)
{
	unsigned int i, j;
	unsigned int len = lines * width;
	char * pos = (char*)mem;

	printf("%s pos =%x\n", pfx, pos);

	for(i = 0; i < lines; i++)
	{
		for(j = 0; j < width; j++)
		{
			if(isprint(*pos)) /* printable char */
			{
				putchar(*pos);
			}
			else /* other char */
			{
				putchar('.');
			}
			pos++;
		}
		putchar('\n');
	}
}


static void init_video_dev(void)
{
	int ret;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_control ctrl;

	vid_fd = open(VIDEO_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
	perror_exit(vid_fd < 0, "open");

	ret = ioctl(vid_fd, VIDIOC_QUERYCAP, &cap);
	perror_exit(ret != 0, "ioctl");

	/* Not any more
	if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M)) {
		fprintf(stderr, "Device does not support V4L2_CAP_VIDEO_M2M\n");
		exit(EXIT_FAILURE);
	}
	 */

	if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) {
		fprintf(stderr, "Device does not support V4L2_CAP_VIDEO_M2M_MPLANE\n");
		exit(EXIT_FAILURE);
	}

	/* The data we send to the device/driver */
	fmt.type 		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	fmt.fmt.pix_mp.width	= width;
	fmt.fmt.pix_mp.height	= height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV; // Single Plane Used

	fmt.fmt.pix_mp.field	= V4L2_FIELD_NONE;

	ret = ioctl(vid_fd, VIDIOC_S_FMT, &fmt);
	perror_exit(ret != 0, "ioctl s_fmt video_output");

	debug("Output FMT %x : WxH %dx%d : Field %x\n",
			fmt.fmt.pix_mp.pixelformat,
			fmt.fmt.pix_mp.width,
			fmt.fmt.pix_mp.height,
			fmt.fmt.pix_mp.field);

	/* Data we receive */
	fmt.type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.width	= width;
	fmt.fmt.pix_mp.height	= height;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_RGB565X;
	//fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_UYVY; // Can see the chars move
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUYV; // Single Plane Used

	fmt.fmt.pix_mp.field	= V4L2_FIELD_NONE; // Do we need to set these?

	ret = ioctl(vid_fd, VIDIOC_S_FMT, &fmt);
	perror_exit(ret != 0, "ioctl s_fmt video_capture");

	debug("Capture FMT %x : WxH %dx%d : Field %x\n",
			fmt.fmt.pix_mp.pixelformat,
			fmt.fmt.pix_mp.width,
			fmt.fmt.pix_mp.height,
			fmt.fmt.pix_mp.field);



}

static char * content_string = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";

static char get_content_char(unsigned int n)
{
	int pos = n % 32;
	return content_string[pos];
}

static void gen_src_buf(void *p, size_t size)
{
	uint8_t val;
	char * buffer = p;
	int i;

	for (i=0; i < size; i++)
		*buffer++ = get_content_char(i);
}

static void gen_dst_buf(void *p, size_t size)
{
	/* White */
	memset(p, 255, 0);
}

static int read_frame(int last)
{
	int ret;
	int j;

	int n, bytesused;

	ret = dequeue_output(&n);
	if (ret)
		return ret;


	/* Verify we've got a correct buffer */
	assert(n < num_src_bufs);

	/* Enqueue back the buffer (note that the index is preserved) */
	if (!last) {
		gen_src_buf(p_src_buf[n], src_buf_size[n]);
		queue_buffer(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP,
				n, src_buf_size[n]);

		debug("Re-queueing Source buffer\n");
		if (hex_not_draw)
			hexdump(p_src_buf[n], 32, "SrcBuf:");
		else
			draw_frame(p_src_buf[n], 8, "SrcBuf:");

	}


	debug("Dequeuing destination buffer\n");
	ret = dequeue_capture(&n, &bytesused);
	if (ret)
		return ret;

	if (bytesused == 0) {
		printf("Capture finished 0 bytes used\n");
	}

	debug("Dequeued dst buffer, index: %d\n", n);
	/* Verify we've got a correct buffer */
	assert(n < num_dst_bufs);

	debug("Current buffer in the transaction: %d\n", curr_buf);
	// NO FB OUT p_fb += curr_buf * (height / translen) * fb_line_w;

	++curr_buf;
	//if (curr_buf >= translen)
	//	curr_buf = 0;

	/* Display results
	for (j = 0; j < height / translen; ++j) {
		memcpy(p_fb, (void *)p_dst_buf[buf.index], fb_buf_w);
		p_fb += fb_line_w;
	} No Display for us ... We'll just print stats of the buffer */

	//debug("Buffer: %60s\n", p_dst_buf[buf.index]);
	if (hex_not_draw)
		hexdump(p_dst_buf[n], 32, "DstBuf:");
	else
		draw_frame(p_dst_buf[n], 8, "DstBuf:");

	debug("DstBuf[%d] CRC: 0x%x\n", n,
			crc8(0x00, p_dst_buf[n],
				   dst_buf_size[n]) );


	/* Enqueue back the buffer */
	if (!last) {
		gen_dst_buf(p_dst_buf[n], dst_buf_size[n]);
		queue_buffer(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP,
				n, dst_buf_size[n]);

		debug("Enqueued back dst buffer\n");
	}

	return 0;
}


int queue_buffer(int type, int memory, int index, int len)
{
	/* Using the mplane API for single planes so far */
	struct v4l2_buffer buf = { 0 };
	struct v4l2_plane planes[1] = { 0 };
	int ret;

	fprintf(stderr, "QBUF type=%d idx=%d: size (%d) %m\n",
			type, index, len);

	buf.type	= type;
	buf.memory	= memory;
	buf.index	= index;
	buf.m.planes 	= planes;
	buf.length	= 1;

	buf.m.planes[0].length = len;
	buf.m.planes[0].bytesused = len;

	ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
	if (ret)
		fprintf(stderr, "Failed to QBUF type=%d idx=%d: size (%d) %m\n",
				type, index, len);

	perror_exit(ret != 0, "ioctl");

	return ret;
}

/* directly from m2mtest */
int dequeue_output(int *n)
{
	struct v4l2_buffer qbuf = { 0, };
	struct v4l2_plane planes[2] = { 0, };

	qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.m.planes = planes;
	qbuf.length = 1;

	if (ioctl(vid_fd, VIDIOC_DQBUF, &qbuf)) {
		fprintf(stderr, "Output dequeue error: %m\n");
		return -1;
	}

	printf("Dequeued output buffer %d\n", qbuf.index);
	*n = qbuf.index;
	return 0;
}
/* directly from m2mtest */
int dequeue_capture(int *n, uint32_t *bytesused)
{
	struct v4l2_buffer qbuf = { 0, };
	struct v4l2_plane planes[2] = { 0, };

	qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	qbuf.memory = V4L2_MEMORY_MMAP;
	qbuf.m.planes = planes;
	qbuf.length = 1;

	if (ioctl(vid_fd, VIDIOC_DQBUF, &qbuf)) {
		fprintf(stderr, "Capture dequeue error: %m\n");
		return -1;
	}

	printf("Dequeued capture buffer %d, bytesused %d\n", qbuf.index, qbuf.m.planes[0].bytesused);
	*n = qbuf.index;
	*bytesused = qbuf.m.planes[0].bytesused;
	return 0;
}

void help(char ** argv)
{
	char * searched = strrchr(argv[0], '/');
	char * appname = searched ? searched + 1 : argv[0];

	printf("%s: \n", appname);
	printf("--width/-w  :    Set width [%d]\n", width);
	printf("--height/-h :    Set height [%d]\n", height);
	printf("--num_frames/-n: Number of frames to process [%d]\n", num_frames);
	printf("--hexdump/x :    Hexdump instead of draw\n");
	printf("--help/-?   :    Display this help\n");

	printf("\n");
}

int process_arguments(int argc, char ** argv)
{
	int option;

	static struct option long_options[] = {
		/*  { .name, .has_arg, .flag, .val } */
		{"width",	required_argument,	0, 'w'},
		{"height", 	required_argument,	0, 'h'},
		{"help",  	no_argument, 		0, '?'},
		{"num_frames",  required_argument,	0, 'n'},
		{"hexdump",	no_argument,		0, 'x'},
		{0, 0, 0, 0}
	};

	while ((option = getopt_long(argc, argv,
			"w:h:n:x?",
			long_options, NULL)) != -1) {

		switch (option) {
		case 'w':
			width = atoi(optarg);
			break;
		case 'h':
			height = atoi(optarg);
			break;
		case 'n':
			num_frames = atoi(optarg);
			break;
		case 'x':
			hex_not_draw = 1;
			break;
		default:
		case '?':
			help(argv);
			exit(0);
			break;
		}
	}
	return 0;

}

int main(int argc, char ** argv)
{
	struct v4l2_buffer buf;
	struct v4l2_requestbuffers reqbuf;
	enum v4l2_buf_type type;
	int last = 0;
	int ret = 0;
	int i;

	prctl(PR_SET_NAME, "fdp1-tester");

	process_arguments(argc, argv);

	init_video_dev();


	memzero(reqbuf);
	reqbuf.count	= NUM_BUFS;
	reqbuf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	reqbuf.memory	= V4L2_MEMORY_MMAP;
	ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
	perror_exit(ret != 0, "ioctl");
	num_src_bufs = reqbuf.count;
	debug("Got %d src buffers\n", num_src_bufs);

	reqbuf.count	= NUM_BUFS;
	reqbuf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
	perror_exit(ret != 0, "ioctl");
	num_dst_bufs = reqbuf.count;
	debug("Got %d dst buffers\n", num_dst_bufs);

	for (i = 0; i < num_src_bufs; ++i) {
		struct v4l2_plane planes[1];
		buf = (struct v4l2_buffer){ 0, }; /* Zero the buffer */

		buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;
		buf.m.planes	= planes;
		buf.length	= 1;

		ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("QUERYBUF returned offset: %x : buf.length %d\n",
				buf.m.planes[0].m.mem_offset, buf.m.planes[0].length);

		src_buf_size[i] = buf.m.planes[0].length;
		p_src_buf[i] = mmap(NULL, buf.m.planes[0].length,
				  PROT_READ | PROT_WRITE, MAP_SHARED, vid_fd,
				  buf.m.planes[0].m.mem_offset);

		perror_exit(MAP_FAILED == p_src_buf[i], "mmap");
	}
	for (i = 0; i < num_dst_bufs; ++i) {
		struct v4l2_plane planes[1];
		buf = (struct v4l2_buffer){ 0, }; /* Zero the buffer */
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;
		buf.m.planes	= planes;
		buf.length	= 1;

		ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("QUERYBUF returned offset: %x\n", buf.m.offset);

		dst_buf_size[i] = buf.m.planes[0].length;
		p_dst_buf[i] = mmap(NULL, buf.m.planes[0].length,
				  PROT_READ | PROT_WRITE, MAP_SHARED, vid_fd,
				  buf.m.planes[0].m.mem_offset);
		perror_exit(MAP_FAILED == p_dst_buf[i], "mmap");
	}

	/* Queue buffers for Output */
	for (i = 0; i < num_src_bufs; ++i) {

		gen_src_buf(p_src_buf[i], src_buf_size[i]);

		ret = queue_buffer(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2_MEMORY_MMAP,
				i, src_buf_size[i]);

		debug("Queued OUTPUT buffer %d\n", i);

		if (hex_not_draw)
			hexdump(p_src_buf[i], 32, "SrcBuf:");
		else
			draw_frame(p_src_buf[i], 8, "SrcBuf:");

	}

	/* Start stream for Output */
	type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ret = ioctl(vid_fd, VIDIOC_STREAMON, &type);
	debug("STREAMON OUTPUT (%d): %d\n", VIDIOC_STREAMON, ret);
	perror_exit(ret != 0, "ioctl streamon output");

	/* Queue buffers for Capture */
	for (i = 0; i < num_dst_bufs; ++i) {
		ret = queue_buffer(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2_MEMORY_MMAP,
				i, dst_buf_size[i]);

		debug("Queued CAPTURE buffer %d\n", i);
		if (hex_not_draw)
			hexdump(p_dst_buf[i], 32, "DstBuf:");
		else
			draw_frame(p_dst_buf[i], 8, "DstBuf:");

	}

	/* Start Stream for Capture */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ret = ioctl(vid_fd, VIDIOC_STREAMON, &type);
	debug("STREAMON CAPTURE (%d): %d\n", VIDIOC_STREAMON, ret);
	perror_exit(ret != 0, "ioctl streamon capture");

	/* Start reading / processing */
	while (num_frames) {
		fd_set read_fds;
		int r;

		FD_ZERO(&read_fds);
		FD_SET(vid_fd, &read_fds);

		debug("Before select\n");
		r = select(vid_fd + 1, &read_fds, NULL, NULL, 0);
		perror_exit(r < 0, "select");
		debug("After select\n");

		if (num_frames == 1)
			last = 1;
		if (read_frame(last)) {
			fprintf(stderr, "Read frame failed\n");
			break;
		}
		--num_frames;
		printf("FRAMES LEFT: %d\n", num_frames);
	}

done:
	close(vid_fd);
//	close(fb_fd);

	for (i = 0; i < num_src_bufs; ++i)
		munmap(p_src_buf[i], src_buf_size[i]);

	for (i = 0; i < num_dst_bufs; ++i)
		munmap(p_dst_buf[i], dst_buf_size[i]);

	return ret;
}
