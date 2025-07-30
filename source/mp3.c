#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp3.h"
#include "playback.h"

static size_t*			buffSize;
static mpg123_handle	*mh = NULL;
static uint32_t			rate;
static uint8_t			channels;

static int initMp3(const char* file);
static uint32_t rateMp3(void);
static uint8_t channelMp3(void);
static uint64_t decodeMp3(void* buffer);
static void exitMp3(void);
static size_t getFileSamplesMp3(void);

/**
 * Set decoder parameters for MP3.
 *
 * \param	decoder Structure to store parameters.
 */
void setMp3(struct decoder_fn* decoder)
{
	decoder->init = &initMp3;
	decoder->rate = &rateMp3;
	decoder->channels = &channelMp3;
	/*
	 * buffSize changes depending on input file. So we set buffSize later when
	 * decoder is initialised.
	 */
	buffSize = &(decoder->buffSize);
	decoder->decode = &decodeMp3;
	decoder->exit = &exitMp3;
	decoder->getFileSamples = &getFileSamplesMp3;
}

static size_t getFileSamplesMp3(void)
{
	off_t len = mpg123_length(mh);
	if(len == MPG123_ERR)
		return 0;
	
	return len * (size_t)channels;
}

/**
 * Initialise MP3 decoder.
 *
 * \param	file	Location of MP3 file to play.
 * \return			0 on success, else failure.
 */
int initMp3(const char* file)
{
	int err = 0;
	int encoding = 0;

	if((err = mpg123_init()) != MPG123_OK)
		return err;

	if((mh = mpg123_new(NULL, &err)) == NULL)
	{
		printf("Error: %s\n", mpg123_plain_strerror(err));
		return err;
	}

	if(mpg123_open(mh, file) != MPG123_OK ||
			mpg123_getformat(mh, (long *) &rate, (int *) &channels, &encoding) != MPG123_OK)
	{
		printf("Trouble with mpg123: %s\n", mpg123_strerror(mh));
		return -1;
	}

	/*
	 * Ensure that this output format will not change (it might, when we allow
	 * it).
	 */
	mpg123_format_none(mh);
	mpg123_format(mh, rate, channels, encoding);

	/*
	 * Buffer could be almost any size here, mpg123_outblock() is just some
	 * recommendation. The size should be a multiple of the PCM frame size.
	 */
	*buffSize = mpg123_outblock(mh) * 16;

	return 0;
}

/**
 * Get sampling rate of MP3 file.
 *
 * \return	Sampling rate.
 */
uint32_t rateMp3(void)
{
	return rate;
}

/**
 * Get number of channels of MP3 file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelMp3(void)
{
	return channels;
}

/**
 * Decode part of open MP3 file.
 *
 * \param buffer	Decoded output.
 * \return			Samples read for each channel.
 */
uint64_t decodeMp3(void* buffer)
{
	size_t done = 0;
	mpg123_read(mh, buffer, *buffSize, &done);
	return done / (sizeof(int16_t));
}

/**
 * Free MP3 decoder.
 */
void exitMp3(void)
{
	mpg123_close(mh);
	mpg123_delete(mh);
	mpg123_exit();
}

/**
 * Check if a file is a valid MP3 file.
 * 
 * \param path	Path to the MP3 file.
 * \return		0 if valid MP3, 1 if not valid.
 */
int isMp3(const char *path)
{
	int err;
	int result = 1;
	mpg123_handle *mh = NULL;
	long rate;
	int channels, encoding;

	// Initialise the library
	if (mpg123_init() != MPG123_OK)
		goto out;

	// Create a decoder handle
	mh = mpg123_new(NULL, &err);
	if (!mh)
		goto exit_init;

	// Try opening the file
	err = mpg123_open(mh, path);
	if (err != MPG123_OK)
		goto exit_handle;

	// Query the decoded format
	if (mpg123_getformat(mh, &rate, &channels, &encoding) != MPG123_OK)
		goto close_handle;

	// Parse first frame in file
	err = mpg123_framebyframe_next(mh);
	if (err != MPG123_OK)
	{
		// If we can't read the first frame, it's not a valid MP3
		goto close_handle;
	}

	// All checks passed: valid MP3
	result = 0;

close_handle:
	mpg123_close(mh);

exit_handle:
	mpg123_delete(mh);

exit_init:
	mpg123_exit();

out:
	return result;
}

