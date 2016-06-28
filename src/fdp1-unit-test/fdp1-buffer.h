/*
 * FDP1 Unit Test Utility
 *      Author: Kieran Bingham
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */

#ifndef _FDP1_BUFFER_H_
#define _FDP1_BUFFER_H_

void fdp1_fill_buffer(struct fdp1_v4l2_buffer * buffer);
void fdp1_clear_buffer(struct fdp1_v4l2_buffer * buffer);

#endif /* _FDP1_BUFFER_H_ */
