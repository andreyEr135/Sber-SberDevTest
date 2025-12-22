#ifndef versionH
#define versionH
//-------------------------------------------------------------

#include <time.h>
#include <sys/time.h>
#include <stdint.h>

typedef struct
{
	char     name[64];
	int      v[2];
	int      build;
	uint32_t crc32;
	time_t   time;
}
TVERSION;

//-------------------------------------------------------------
#endif

