#if defined __gnu_linux__
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "file.h"
#include "flac.h"
#include "mp3.h"
#include "opus.h"
#include "playback.h"
#include "vorbis.h"
#include "wav.h"

/**
 * Test the various decoder modules in ctrmus.
 */
int main(int argc, char *argv[])
{
	struct decoder_fn	decoder;
	enum file_types		ft;
	const char			*file = argv[1];
	int16_t				*buffer = NULL;
	FILE				*out;

	if(argc != 2)
	{
		puts("FILE is required.");
		printf("%s FILE\n", argv[0]);
		return 0;
	}

	switch(ft = getFileType(file))
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
			puts("Unsupported file.");
			goto err;
	}

	printf("Type: %s\n", fileToStr(ft));

	if((*decoder.init)(file) != 0)
	{
		puts("Unable to initialise decoder.");
		goto err;
	}

	if((*decoder.channels)() > 2 || (*decoder.channels)() < 1)
	{
		puts("Unable to obtain number of channels.");
		goto err;
	}

	out = fopen("out", "wb");
	buffer = malloc(decoder.buffSize * sizeof(int16_t));

	while(true)
	{
		size_t read = (*decoder.decode)(&buffer[0]);

		if(read <= 0)
			break;

		fwrite(buffer, read * sizeof(int16_t), 1, out);
	}

	(*decoder.exit)();
	free(buffer);
	fclose(out);

	return 0;

err:
	puts("Error");
	return -1;
}

#else
#pragma message ( "Test ignored for 3DS build." )
#endif
