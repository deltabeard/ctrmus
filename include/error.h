#if defined __arm__
#include <3ds.h>
#endif

/* Errors that can't be explained with errno */
#define NDSP_INIT_FAIL			1000
#define DECODER_INIT_FAIL		1001
#define FILE_NOT_SUPPORTED		1002
#define UNSUPPORTED_CHANNELS	1003

#if defined __arm__

/**
 * Struct to help error handling across threads.
 */
struct errInfo_t
{
	/* errno code or from defines listed above */
	volatile int*	error;

	/* Extra information regarding error (Must be NULL if unused) */
	volatile char*	errstr;

	/* Event to trigger on error */
	Handle*			failEvent;
};

/**
 * Return string describing error number. Extends strerror to include some
 * custom errors used in ctrmus.
 *
 * \param err	Error number.
 */
char* ctrmus_strerror(int err);

#endif
