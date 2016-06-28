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

static int fdp1_open_close(struct fdp1_context * fdp1)
{
	int ret;

	start_test(fdp1, "Open Close");

	struct fdp1_v4l2_dev * dev = fdp1_v4l2_open(fdp1);

	if (!dev)
		return TEST_FAIL;

	return fdp1_v4l2_close(dev);
}


int fdp1_open_tests(struct fdp1_context * fdp1)
{
	unsigned int fail = 0;

	/* Simple open then close test */
	fail += fdp1_open_close(fdp1);

	return fail;
}
