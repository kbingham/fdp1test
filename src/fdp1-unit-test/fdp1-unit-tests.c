/*
 * FDP1 Unit Test Utility
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



#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <sys/mman.h>

#include "fdp1-unit-test.h"

#define memzero(x)\
	memset(&(x), 0, sizeof (x));

/* Options filled with defaults */
static struct fdp1_context fdp1_ctx = {
	.dev = 0,
	.width = 128,
	.height = 80,
	.num_frames = 30,
	.verbose = false,
	.interlaced_tests = 0,
};

void help(char ** argv, struct fdp1_context * fdp1)
{
	printf("%s: \n", fdp1->appname);
	printf("--device/-d     :  Use device /dev/videoX (%d)\n", fdp1->dev);
	printf("--width/-w      :  Set width [%d]\n", fdp1->width);
	printf("--height/-h     :  Set height [%d]\n", fdp1->height);
	printf("--num_frames/-n :  Number of frames to process [%d]\n", fdp1->num_frames);
	printf("--hexdump/x     :  Hexdump instead of draw\n");
	printf("--verbose/v     :  Verbose test output [%d]\n", fdp1->verbose);
	printf("--help/-?       :  Display this help\n");

	printf("\n");
}

int process_arguments(int argc, char ** argv, struct fdp1_context * fdp1)
{
	int option;

	static struct option long_options[] = {
		/*  { .name, .has_arg, .flag, .val } */
		{"device",	required_argument,	0, 'd'},
		{"width",	required_argument,	0, 'w'},
		{"height", 	required_argument,	0, 'h'},
		{"help",  	no_argument, 		0, '?'},
		{"num_frames",  required_argument,	0, 'n'},
		{"hexdump",	no_argument,		0, 'x'},
		{"verbose",	no_argument,		0, 'v'},
		{"interlaced",	no_argument,		0, 'i'},
		{0, 0, 0, 0}
	};

	while ((option = getopt_long(argc, argv,
			"d:w:h:n:xvi?",
			long_options, NULL)) != -1) {

		switch (option) {
		case 'd':
			fdp1->dev = atoi(optarg);
			break;
		case 'w':
			fdp1->width = atoi(optarg);
			break;
		case 'h':
			fdp1->height = atoi(optarg);
			break;
		case 'n':
			fdp1->num_frames = atoi(optarg);
			break;
		case 'x':
			fdp1->hex_not_draw = 1;
			break;
		case 'v':
			fdp1->verbose++;
			break;
		case 'i':
			fdp1->interlaced_tests = 1;
			break;
		default:
		case '?':
			help(argv, fdp1);
			exit(0);
			break;
		}
	}

	return 0;
}

int main(int argc, char ** argv)
{
	unsigned int fail = 0;

	char * searched = strrchr(argv[0], '/');
	fdp1_ctx.appname = searched ? searched + 1 : argv[0];

	process_arguments(argc, argv, &fdp1_ctx);

	/* Ideally these would be automatically iterated */
	if (fdp1_ctx.interlaced_tests) {
		fail += fdp1_deinterlace(&fdp1_ctx);
	} else {
		fail += fdp1_open_tests(&fdp1_ctx);
		fail += fdp1_allocation_tests(&fdp1_ctx);
		fail += fdp1_stream_on_tests(&fdp1_ctx);
		fail += fdp1_progressive(&fdp1_ctx);
	}

	printf("%s: Test results: %d tests failed\n", fdp1_ctx.appname, fail);
}
