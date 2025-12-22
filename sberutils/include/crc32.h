#ifndef crc32H
#define crc32H
//---------------------------------------------------------------------------

#include <stdint.h>
#include <stddef.h>

uint32_t fcrc32( uint32_t crc, const void *buf, size_t size );

int fcrc32file( uint32_t& crc, const char *filename );

//---------------------------------------------------------------------------
#endif

