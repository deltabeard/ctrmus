#include <stdlib.h>
#include <string.h>

#include "opus.h"
#include "playback.h"

static OggOpusFile*		opusFile;
static const OpusHead*	opusHead;
static const size_t		buffSize = 32 * 1024;

static int initOpus(const char* file);
static uint32_t rateOpus(void);
static uint8_t channelOpus(void);
static uint64_t decodeOpus(void* buffer);
static void exitOpus(void);
static uint64_t fillOpusBuffer(int16_t* bufferOut);
static size_t getFileSamplesOpus(void);

/**
 * Set decoder parameters for Opus.
 *
 * \param	decoder	Structure to store parameters.
 */
void setOpus(struct decoder_fn* decoder)
{
	decoder->init = &initOpus;
	decoder->rate = &rateOpus;
	decoder->channels = &channelOpus;
	decoder->buffSize = buffSize;
	decoder->decode = &decodeOpus;
	decoder->exit = &exitOpus;
	decoder->getFileSamples = &getFileSamplesOpus;
}

static size_t getFileSamplesOpus(void)
{
	ogg_int64_t len = op_pcm_total(opusFile, -1);

	if(len == OP_EINVAL)
		return 0;

	return len * (size_t)channelOpus();
}

/**
 * Initialise Opus decoder.
 *
 * \param	file	Location of opus file to play.
 * \return			0 on success, else failure.
 */
int initOpus(const char* file)
{
	int err = 0;

	if((opusFile = op_open_file(file, &err)) == NULL)
		goto out;

	if((err = op_current_link(opusFile)) < 0)
		goto out;

	opusHead = op_head(opusFile, err);

out:
	return err;
}

/**
 * Get sampling rate of Opus file.
 *
 * \return	Sampling rate. Should be 48000.
 */
uint32_t rateOpus(void)
{
	return 48000;
}

/**
 * Get number of channels of Opus file.
 *
 * \return	Number of channels for opened file, so always be 2.
 */
uint8_t channelOpus(void)
{
	/* Opus decoder always returns stereo stream */
	return 2;
}

/**
 * Decode part of open Opus file.
 *
 * \param buffer	Decoded output.
 * \return			Samples read for each channel. 0 for end of file, negative
 *					for error.
 */
uint64_t decodeOpus(void* buffer)
{
	return fillOpusBuffer(buffer);
}

/**
 * Free Opus decoder.
 */
void exitOpus(void)
{
	op_free(opusFile);
}

/**
 * Decode Opus file to fill buffer.
 *
 * \param bufferOut		Pointer to buffer.
 * \return				Samples read per channel.
 */
uint64_t fillOpusBuffer(int16_t* bufferOut)
{
	uint64_t samplesRead = 0;
	int samplesToRead = buffSize;

	while(samplesToRead > 0)
	{
		int samplesJustRead = op_read_stereo(opusFile, bufferOut,
				samplesToRead > 120*48*2 ? 120*48*2 : samplesToRead);

		if(samplesJustRead < 0)
			return samplesJustRead;
		else if(samplesJustRead == 0)
		{
			/* End of file reached. */
			break;
		}

		samplesRead += samplesJustRead * 2;
		samplesToRead -= samplesJustRead * 2;
		bufferOut += samplesJustRead * 2;
	}

	return samplesRead;
}

/**
 * Checks if the input file is Opus.
 *
 * \param in	Input file.
 * \return		0 if Opus file, else not or failure.
 */
int isOpus(const char* in)
{
	int err = 0;
	OggOpusFile* opusTest = op_test_file(in, &err);

	op_free(opusTest);
	return err;
}
