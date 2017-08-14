#if defined __gnu_linux__
#include <stdint.h>
#include <stdio.h>

#include "flac.h"
#include "mp3.h"
#include "opus.h"
#include "playback.h"
#include "vorbis.h"
#include "wav.h"
#include "playback.h"

/**
 * Test the various decoder modules in ctrmus.
 */
int main(int argc, char *argv[])
{
	struct decoder_fn decoder;

	if(argc != 2)
	{
		puts("FILE is required.");
		printf("%s FILE\n", argv[0]);
		return 0;
	}
#if 0
	switch(getFileType(argv[1]))
	{
		case FILE_TYPE_WAV:
			setWav(&decoder);
			break;

		case FILE_TYPE_FLAC:
			setFlac(&decoder);
			break;

		case FILE_TYPE_OPUS:
			setOpus(&decoder);
			break;

		case FILE_TYPE_MP3:
			setMp3(&decoder);
			break;

		case FILE_TYPE_VORBIS:
			setVorbis(&decoder);
			break;

		default:
			goto err;
	}
#endif

	return 0;

err:
	puts("Error");
	return -1;
}

#else
#pragma message ( "Test ignored for 3DS build." )
#endif
