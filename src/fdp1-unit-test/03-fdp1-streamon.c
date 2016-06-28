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


int fdp1_stream_on_tests(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	fail += fdp1_test_m2m_object(fdp1);

	return fail;
}
