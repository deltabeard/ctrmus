#include <3ds.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <errno.h>
//#include <string.h>

#define DR_FLAC_IMPLEMENTATION
#include <../source/dr_libs/dr_flac.h>

#define SAMPLES_TO_READ	128 * 1024
#define CHANNEL			0x08

int playFlac(const char* in)
{
	drflac*		pFlac = drflac_open_file(in);
	/* Number of samples actually read */
	uint64_t	nsamples = 0;
	int			chunkSize = SAMPLES_TO_READ * sizeof(s16);
	s16*		buffer1 = linearAlloc(SAMPLES_TO_READ * sizeof(s16));
	s16*		buffer2 = linearAlloc(SAMPLES_TO_READ * sizeof(s16));
	uint64_t	readsamples = 0;
	ndspWaveBuf waveBuf[2];
	bool playing = true;
	bool lastbuf = false;

	if (pFlac == NULL) {
		return -1;
	}

	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		goto out;
	}

	printf("\nRate: %lu\tChan: %d\n", pFlac->sampleRate,
			pFlac->channels);

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, pFlac->sampleRate);
	ndspChnSetFormat(CHANNEL, pFlac->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
	memset(waveBuf, 0, sizeof(waveBuf));
	waveBuf[0].nsamples = drflac_read_s16(pFlac, chunkSize, buffer1) / pFlac->channels;
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
	//waveBuf[1].nsamples = drflac_read_s16(pFlac, chunkSize, buffer2) / pFlac->channels;
	//waveBuf[1].data_vaddr = &buffer2[0];
	//ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	printf("Playing %s\n", in);
	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false)
	{}

	while(true || playing == false || ndspChnIsPlaying(CHANNEL) == true)
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

		if(kDown & KEY_A)
			playing = !playing;

		if(playing == false || lastbuf == true)
		{
			printf("\33[2K\rPaused");
			continue;
		}

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			printf("\rBeginning Decode.");
			read = drflac_read_s16(pFlac, chunkSize, buffer1);
			printf("\rDone decode.");

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < chunkSize)
				waveBuf[0].nsamples = read / pFlac->channels;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

#if 0
		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			read = drflac_read_s16(pFlac, chunkSize, buffer2);

			if(read == 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < chunkSize)
				waveBuf[1].nsamples = read / pFlac->channels;

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}
		// TODO: Remove this printf.
		// \33[2K clears the current line.
		printf("\33[2K\rRead: %u\tBuf0: %s\tBuf1: %s", read,
				waveBuf[0].status == NDSP_WBUF_QUEUED ? "Q" : "P",
				waveBuf[1].status == NDSP_WBUF_QUEUED ? "Q" : "P");
#endif

		DSP_FlushDataCache(buffer1, chunkSize);
		DSP_FlushDataCache(buffer2, chunkSize);
	}

	printf("\nEnd of file.");

out:
	printf("\nStopping Flac playback.\n");
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	linearFree(buffer1);
	linearFree(buffer2);
	drflac_close(pFlac);
	return 0;
}
