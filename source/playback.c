#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "ffmpeg.h"
#include "playback.h"

int buffSize = 2048;

int playFile(const char* file)
{
	int16_t*		buffer1 = NULL;
	int16_t*		buffer2 = NULL;
	ndspWaveBuf		waveBuf[2];
	bool			playing = true;
	bool			lastbuf = false;
	int				ret;

	// TODO: Initialise FFMPEG
	if((ret = initffmpeg(file)) != 0)
	{
		printf("Error initialising decoder: %d\n", ret);
		return -1;
	}

	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		goto out;
	}

	buffer1 = linearAlloc(buffSize * sizeof(int16_t));
	buffer2 = linearAlloc(buffSize * sizeof(int16_t));

#ifdef DEBUG
	printf("\nRate: %lu\tChan: %d\n", rateffmpeg(), channelffmpeg());
#endif

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, rateffmpeg());
	ndspChnSetFormat(CHANNEL,
			channelffmpeg() == 2 ? NDSP_FORMAT_STEREO_PCM16 :
			NDSP_FORMAT_MONO_PCM16);

	memset(waveBuf, 0, sizeof(waveBuf));
	waveBuf[0].nsamples = decodeffmpeg(&buffer1[0]) / channelffmpeg();
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);

	waveBuf[1].nsamples = decodeffmpeg(&buffer2[0]) / channelffmpeg();
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", file);

	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false);

	while(playing == false || ndspChnIsPlaying(CHANNEL) == true)
	{
		u32 kDown;

		/* Number of bytes read from file.
		 * Static only for the purposes of the printf debug at the bottom.
		 */
		static size_t read = 0;

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
			read = decodeffmpeg(&buffer1[0]);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < buffSize)
				waveBuf[0].nsamples = read / channelffmpeg();

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			read = decodeffmpeg(&buffer2[0]);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < buffSize)
				waveBuf[1].nsamples = read / channelffmpeg();

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}

		DSP_FlushDataCache(buffer1, buffSize * sizeof(int16_t));
		DSP_FlushDataCache(buffer2, buffSize * sizeof(int16_t));
	}

out:
	printf("\nStopping playback.\n");
	exitffmpeg();
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	linearFree(buffer1);
	linearFree(buffer2);
	return 0;
}
