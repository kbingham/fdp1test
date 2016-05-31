/*
 * CRC8 functionality taken from
 * http://stackoverflow.com/questions/15169387/definitive-crc-for-c
 */

#include <stddef.h>

#ifndef __CRC8__
#define __CRC8__

unsigned crc8(unsigned crc, unsigned char *data, size_t len);

#endif
