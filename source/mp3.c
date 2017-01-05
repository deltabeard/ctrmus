#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "mp3.h"

int playMp3(const char* in)
{
	int				err = 0;
	mpg123_handle	*mh = NULL;
	size_t			buffer_size = 0;
	size_t			done = 0;
	long			rate = 0;
	int				channels = 0;
	int				encoding = 0;
	unsigned char*	buffer1 = NULL;
	unsigned char*	buffer2 = NULL;
	int				ret = 0;
	ndspWaveBuf		waveBuf[2];
	bool			playing = true;
	bool			lastbuf = false;

	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		goto out;
	}

	if((err = mpg123_init()) != MPG123_OK)
		return err;

	if((mh = mpg123_new(NULL, &err)) == NULL)
	{
		printf("Error: %s\n", mpg123_plain_strerror(err));
		goto err;
	}

	if(mpg123_open(mh, in) != MPG123_OK ||
			mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK)
	{
		printf("Trouble with mpg123: %s\n", mpg123_strerror(mh));
		goto err;
	}

	/* Ensure that this output format will not change
	   (it might, when we allow it). */
	mpg123_format_none(mh);
	mpg123_format(mh, rate, channels, encoding);

	/* Buffer could be almost any size here, mpg123_outblock() is just some
	   recommendation. The size should be a multiple of the PCM frame size. */
	buffer_size = mpg123_outblock(mh) * 16;
	buffer1 = linearAlloc(buffer_size);
	buffer2 = linearAlloc(buffer_size);

	/* I'm not sure if this error will ever occur. */
	if(channels > 2)
	{
		printf("Invalid number of channels %d\n", channels);
		goto err;
	}

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, rate);
	ndspChnSetFormat(CHANNEL,
			channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

	memset(waveBuf, 0, sizeof(waveBuf));

	mpg123_read(mh, buffer1, buffer_size, &done);
	waveBuf[0].nsamples = done / (sizeof(s16) * channels);
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);

	mpg123_read(mh, buffer2, buffer_size, &done);
	waveBuf[1].nsamples = done / (sizeof(s16) * channels);
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", in);
	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false);

	while(playing == false || ndspChnIsPlaying(CHANNEL) == true)
	{
		u32 kDown;

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_B)
			break;

		if(kDown & (KEY_A | KEY_R))
		{
			playing = !playing;
			printf("\33[2K\r%s", playing == false ? "Paused" : "");
		}

		if(playing == false || lastbuf == true)
			continue;

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			err = mpg123_read(mh, buffer1, buffer_size, &done);

			if(err != MPG123_OK)
			{
				lastbuf = true;
				continue;
			}

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			err = mpg123_read(mh, buffer2, buffer_size, &done);

			if(err != MPG123_OK)
			{
				lastbuf = true;
				continue;
			}

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}

		DSP_FlushDataCache(buffer1, buffer_size);
		DSP_FlushDataCache(buffer2, buffer_size);
	}

out:
	linearFree(buffer1);
	linearFree(buffer2);
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
	printf("\nStopping MP3 playback.\n");
	return ret;

err:
	ret = -1;
	goto out;
}
