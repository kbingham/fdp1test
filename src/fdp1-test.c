/*
 * FDP1 Test Utility.c
 *      Author: kbingham
 *
 * Based on process-vmalloc test utility
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
int width = 640;
int height = 480;
static int vid_fd;
static char *p_src_buf[NUM_BUFS], *p_dst_buf[NUM_BUFS];
static size_t src_buf_size[NUM_BUFS], dst_buf_size[NUM_BUFS];
static uint32_t num_src_bufs = 0, num_dst_bufs = 0;

int curr_buf = 0;
int num_frames = 6;


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
	fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420M;
	fmt.fmt.pix_mp.field	= V4L2_FIELD_INTERLACED;

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
	fmt.fmt.pix_mp.field	= V4L2_FIELD_ANY; // Do we need to set these?

	ret = ioctl(vid_fd, VIDIOC_S_FMT, &fmt);
	perror_exit(ret != 0, "ioctl s_fmt video_capture");

	debug("Capture FMT %x : WxH %dx%d : Field %x\n",
			fmt.fmt.pix_mp.pixelformat,
			fmt.fmt.pix_mp.width,
			fmt.fmt.pix_mp.height,
			fmt.fmt.pix_mp.field);



}

static void gen_src_buf(void *p, size_t size)
{
	uint8_t val;

	val = rand() % 256;
	memset(p, val, size);
}

static void gen_dst_buf(void *p, size_t size)
{
	/* White */
	memset(p, 255, 0);
}

static int read_frame(int last)
{
	struct v4l2_buffer buf;
	int ret;
	int j;
	/// Not for us ... char * p_fb = fb_addr + fb_off;

	memzero(buf);

	buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory	= V4L2_MEMORY_MMAP;

	ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);
	debug("Dequeued source buffer, index: %d\n", buf.index);
	if (ret) {
		switch (errno) {
		case EAGAIN:
			debug("Got EAGAIN\n");
			return 0;

		case EIO:
			debug("Got EIO\n");
			return 0;

		default:
			perror("ioctl");
			return 0;
		}
	}

	/* Verify we've got a correct buffer */
	assert(buf.index < num_src_bufs);

	/* Enqueue back the buffer (note that the index is preserved) */
	if (!last) {
		gen_src_buf(p_src_buf[buf.index], src_buf_size[buf.index]);
		buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.bytesused	= src_buf_size[buf.index];
		ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
		perror_ret(ret != 0, "ioctl");
	}


	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	debug("Dequeuing destination buffer\n");
	ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);
	if (ret) {
		switch (errno) {
		case EAGAIN:
			debug("Got EAGAIN\n");
			return 0;

		case EIO:
			debug("Got EIO\n");
			return 0;

		default:
			perror("ioctl");
			return 1;
		}
	}
	debug("Dequeued dst buffer, index: %d\n", buf.index);
	/* Verify we've got a correct buffer */
	assert(buf.index < num_dst_bufs);

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

	/* Enqueue back the buffer */
	if (!last) {
		gen_dst_buf(p_dst_buf[buf.index], dst_buf_size[buf.index]);
		ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
		perror_ret(ret != 0, "ioctl");
		debug("Enqueued back dst buffer\n");
	}

	return 0;
}

void help(char ** argv)
{
	char * searched = strrchr(argv[0], '/');
	char * appname = searched ? searched + 1 : argv[0];

	printf("%s: \n", appname);
	printf("--width/-w  :    Set width [%d]\n", width);
	printf("--height/-h :    Set height [%d]\n", height);
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
		{0, 0, 0, 0}
	};

	while ((option = getopt_long(argc, argv,
			"w:h:?",
			long_options, NULL)) != -1) {

		switch (option) {
		case 'w':
			width = atoi(optarg);
			break;
		case 'h':
			height = atoi(optarg);
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
	reqbuf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	type		= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory	= V4L2_MEMORY_MMAP;
	ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
	perror_exit(ret != 0, "ioctl");
	num_src_bufs = reqbuf.count;
	debug("Got %d src buffers\n", num_src_bufs);

	reqbuf.count	= NUM_BUFS;
	reqbuf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
	perror_exit(ret != 0, "ioctl");
	num_dst_bufs = reqbuf.count;
	debug("Got %d dst buffers\n", num_dst_bufs);

	for (i = 0; i < num_src_bufs; ++i) {
		buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;

		ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("QUERYBUF returned offset: %x\n", buf.m.offset);

		src_buf_size[i] = buf.length;
		p_src_buf[i] = mmap(NULL, buf.length,
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    vid_fd, buf.m.offset);
		perror_exit(MAP_FAILED == p_src_buf[i], "mmap");
	}
	for (i = 0; i < num_dst_bufs; ++i) {
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;

		ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("QUERYBUF returned offset: %x\n", buf.m.offset);

		dst_buf_size[i] = buf.length;
		p_dst_buf[i] = mmap(NULL, buf.length,
				    PROT_READ | PROT_WRITE, MAP_SHARED,
				    vid_fd, buf.m.offset);
		perror_exit(MAP_FAILED == p_dst_buf[i], "mmap");
	}

	/* Queue buffers for Output */
	for (i = 0; i < num_src_bufs; ++i) {

		gen_src_buf(p_src_buf[i], src_buf_size[i]);

		memzero(buf);
		buf.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;

		ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("Queued OUTPUT buffer %d\n", buf.index);
	}

	/* Start stream for Output */
	type = buf.type;
	ret = ioctl(vid_fd, VIDIOC_STREAMON, &type);
	debug("STREAMON OUTPUT (%d): %d\n", VIDIOC_STREAMON, ret);
	perror_exit(ret != 0, "ioctl streamon output");

	/* Queue buffers for Capture */
	for (i = 0; i < num_dst_bufs; ++i) {
		memzero(buf);
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= i;

		ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
		perror_exit(ret != 0, "ioctl");
		debug("Queued CAPTURE buffer %d\n", buf.index);
	}

	/* Start Stream for Capture */
	type = buf.type;
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
