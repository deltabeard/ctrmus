#include <3ds.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "error.h"
#include "file.h"
#include "flac.h"
#include "mp3.h"
#include "opus.h"
#include "playback.h"
#include "vorbis.h"
#include "wav.h"

static volatile bool stop = true;

/**
 * Pause or play current file.
 *
 * \return	True if paused.
 */
bool togglePlayback(void)
{
	bool paused = ndspChnIsPaused(CHANNEL);
	ndspChnSetPaused(CHANNEL, !paused);
	return !paused;
}

/**
 * Stops current playback. Playback thread should exit as a result.
 */
void stopPlayback(void)
{
	stop = true;
}

/**
 * Returns whether music is playing or paused.
 */
bool isPlaying(void)
{
	return !stop;
}

/**
 * Should only be called from a new thread only, and have only one playback
 * thread at time. This function has not been written for more than one
 * playback thread in mind.
 *
 * \param	infoIn	Playback information.
 */
void playFile(void* infoIn)
{
	struct decoder_fn decoder;
	struct playbackInfo_t* info = infoIn;
	int16_t*		buffer1 = NULL;
	int16_t*		buffer2 = NULL;
	ndspWaveBuf		waveBuf[2];
	bool			lastbuf = false;
	int				ret = -1;
	const char*		file = info->file;
	bool			isNdspInit = false;

	/* Reset previous stop command */
	stop = false;

	switch(getFileType(file))
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

	if(ndspInit() < 0)
	{
		errno = NDSP_INIT_FAIL;
		goto err;
	}

	isNdspInit = true;

	if((ret = (*decoder.init)(file)) != 0)
	{
		errno = DECODER_INIT_FAIL;
		goto err;
	}

	if((*decoder.channels)() > 2 || (*decoder.channels)() < 1)
	{
		errno = UNSUPPORTED_CHANNELS;
		goto err;
	}

	buffer1 = linearAlloc(decoder.buffSize * sizeof(int16_t));
	buffer2 = linearAlloc(decoder.buffSize * sizeof(int16_t));

	ndspChnReset(CHANNEL);
	ndspChnWaveBufClear(CHANNEL);
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	ndspChnSetRate(CHANNEL, (*decoder.rate)());
	ndspChnSetFormat(CHANNEL,
			(*decoder.channels)() == 2 ? NDSP_FORMAT_STEREO_PCM16 :
			NDSP_FORMAT_MONO_PCM16);

	memset(waveBuf, 0, sizeof(waveBuf));
	waveBuf[0].nsamples = (*decoder.decode)(&buffer1[0]) / (*decoder.channels)();
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);

	waveBuf[1].nsamples = (*decoder.decode)(&buffer2[0]) / (*decoder.channels)();
	waveBuf[1].data_vaddr = &buffer2[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);

	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false);

	while(stop == false)
	{
		svcSleepThread(100 * 1000);

		/* When the last buffer has finished playing, break. */
		if(lastbuf == true && waveBuf[0].status == NDSP_WBUF_DONE &&
				waveBuf[1].status == NDSP_WBUF_DONE)
			break;

		if(ndspChnIsPaused(CHANNEL) == true || lastbuf == true)
			continue;

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			size_t read = (*decoder.decode)(&buffer1[0]);

			if(read <= 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < decoder.buffSize)
				waveBuf[0].nsamples = read / (*decoder.channels)();

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
		}

		if(waveBuf[1].status == NDSP_WBUF_DONE)
		{
			size_t read = (*decoder.decode)(&buffer2[0]);

			if(read <= 0)
			{
				lastbuf = true;
				continue;
			}
			else if(read < decoder.buffSize)
				waveBuf[1].nsamples = read / (*decoder.channels)();

			ndspChnWaveBufAdd(CHANNEL, &waveBuf[1]);
		}

		DSP_FlushDataCache(buffer1, decoder.buffSize * sizeof(int16_t));
		DSP_FlushDataCache(buffer2, decoder.buffSize * sizeof(int16_t));
	}

	(*decoder.exit)();
out:
	if(isNdspInit == true)
	{
		ndspChnWaveBufClear(CHANNEL);
		ndspExit();
	}

	delete(info->file);
	linearFree(buffer1);
	linearFree(buffer2);

	/* Signal Watchdog thread that we've stopped playing */
	*info->errInfo->error = -1;
	svcSignalEvent(*info->errInfo->failEvent);

	threadExit(0);
	return;

err:
	*info->errInfo->error = errno;
	svcSignalEvent(*info->errInfo->failEvent);
	goto out;
}
