#include "error.h"
#include <errno.h>
#include <string.h>

/**
 * Return string describing error number. Extends strerror to include some
 * custom errors used in ctrmus.
 *
 * \param err	Error number.
 */
char* ctrmus_strerror(int err)
{
	char* error;

	switch(err)
	{
		case NDSP_INIT_FAIL:
			error = "NDSP Initialisation failed";
			break;

		case DECODER_INIT_FAIL:
			error = "Unable to initialised decoder";
			break;

		case FILE_NOT_SUPPORTED:
			error = "File type is not supported";
			break;

		case UNSUPPORTED_CHANNELS:
			error = "Unsupported number of channels";
			break;

		default:
			error = strerror(err);
			break;
	}

	return error;
}
