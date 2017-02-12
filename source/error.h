#include <3ds.h>

/* Errors that can't be explained with errno */
#define NDSP_INIT_FAIL		1000
#define DECODER_INIT_FAIL	1001
#define FILE_NOT_SUPPORTED	1002

struct errInfo_t
{
	volatile int*	error;
	Handle*			failEvent;
	volatile char*	errstr;
};

char* ctrmus_strerror(int err);
