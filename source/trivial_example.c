/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE libopusfile SOFTWARE CODEC SOURCE CODE. *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE libopusfile SOURCE CODE IS (C) COPYRIGHT 1994-2012           *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************/
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opus/opusfile.h>

// Number of samples per second * channels * size of one sample
#define CHANNEL		0x08
#define BUFFER_SIZE	120*48*2*sizeof(opus_int16)*32

int convOpus(const char* in, const char *outf)
{
	OggOpusFile		*of;
	FILE			*wav;
	ogg_int64_t		duration = 0;
	int				ret;
	int				output_seekable;
	ogg_int64_t		nsamples = 0;
	opus_int16*		pcm = linearAlloc(120*48*2*sizeof(opus_int16)*32);
	ndspWaveBuf		waveBuf;

	of = op_open_file(in, &ret);

	if((wav = fopen(outf, "w+")) == NULL)
	{
		fprintf(stderr, "Failed to open file '%s': %i %s\n",outf,errno, strerror(errno));
		return EXIT_FAILURE;
	}

	if(of == NULL)
	{
		fprintf(stderr, "Failed to open file '%s': %i\n",in,ret);
		return EXIT_FAILURE;
	}

	// Not really required.
	output_seekable = fseek(wav, 0, SEEK_CUR);

	if(op_seekable(of))
	{
		fprintf(stderr, "\nTotal number of links: %i\n", op_link_count(of));
		// Get PCM length of entire stream.
		duration = op_pcm_total(of, -1);
		fprintf(stderr, "Total duration: ");
		fprintf(stderr, " (%li samples @ 48 kHz)\n", (long)duration);
	}
	else if(output_seekable < 0)
		fprintf(stderr,"WARNING: Neither input nor output are seekable.\n");

	/* Initialise DSP stuff */
	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		goto out;
	}

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	/* Polyphase sounds much better than linear or no interpolation */
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, 48000);
	ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);
	memset(&waveBuf, 0, sizeof(waveBuf));
	waveBuf.data_vaddr = &pcm;

	fprintf(stderr, "PCM %d, %d\n", sizeof(pcm), sizeof(*pcm));

	for(;;)
	{
		u32 kDown;
		static int num = 0;
		int semsamples = 0;

		hidScanInput();
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();

		kDown = hidKeysDown();

		if(kDown & KEY_B)
			break;

		/* Returns number of samples decoded. */
		for(int i = 0; i < 32; i++)
		{
			ret = op_read_stereo(of, pcm + (120*48*2*i), 120*48*2);
			nsamples += ret;
			semsamples += ret;
		}


		if(ret < 0)
		{
			fprintf(stderr, "\nError decoding '%s': %i\n", in, ret);
			ret = EXIT_FAILURE;
			break;
		}

		if(ret == 0)
		{
			ret = EXIT_SUCCESS;
			break;
		}


		waveBuf.nsamples = semsamples;

		fprintf(stderr, "Ret: %d, #: %d, Sample: %li", ret, num++,
				(long)semsamples);
		DSP_FlushDataCache(pcm, 120*48*2*sizeof(opus_int16)*32);
		ndspChnWaveBufAdd(CHANNEL, &waveBuf);

		while(waveBuf.status != NDSP_WBUF_DONE)
		{
			u32 kDown;
			hidScanInput();
			kDown = hidKeysDown();

			if(kDown & KEY_B)
				break;

			if(kDown & KEY_START)
				goto out;

			printf("Status: %d:%s\n", waveBuf.status,
					waveBuf.status == NDSP_WBUF_QUEUED ? "Que'd" : "Playing");
		}

#if 0
		// Number of samples read * size of each sample * number of channels
		if(fwrite(pcm, ret * sizeof(opus_int16) * 2, 1, wav) == 0)
		{
			fprintf(stderr, "\nError writing decoded audio data: %s\n",
					strerror(errno));
			ret = EXIT_FAILURE;
			break;
		}

		waveBuf[0].data_vaddr = &pcm[0];
		waveBuf[0].nsamples = ret;
		ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		while(ndspChnIsPlaying(CHANNEL) == false)
		{}
		while(ndspChnIsPlaying(CHANNEL) == true)
		{}
		printf(" Buf0: %s.", waveBuf[0].status == NDSP_WBUF_QUEUED ? "Queued" : "Playing");
#endif
	}

	if(ret == EXIT_SUCCESS)
	{
		fprintf(stderr, "\nDone: played ");
		fprintf(stderr, " (%li samples @ 48 kHz).\n", (long)nsamples);
	}

	if(op_seekable(of) && nsamples != duration)
	{
		fprintf(stderr, "\nWARNING: Number of output samples does not match "
				"declared file duration.\n");
		if(output_seekable < 0)
			fprintf(stderr, "Output WAV file will be corrupt.\n");
	}

	if(fclose(wav) != 0)
		fprintf(stderr, "\nError closing file: %d - %s\n", errno, strerror(errno));

out:
	ndspExit();
	op_free(of);
	linearFree(pcm);

	return ret;
}
