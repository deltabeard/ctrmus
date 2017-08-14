#include <stdbool.h>

#ifndef ctrmus_playback_h
#define ctrmus_playback_h

/* Channel to play music on */
#define CHANNEL	0x08

enum file_types
{
	FILE_TYPE_ERROR = -1,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_VORBIS,
	FILE_TYPE_OPUS,
	FILE_TYPE_MP3
};

struct decoder_fn
{
	int (* init)(const char* file);
	uint32_t (* rate)(void);
	uint8_t (* channels)(void);
	size_t buffSize;
	uint64_t (* decode)(void*);
	void (* exit)(void);
};

struct playbackInfo_t
{
	char*				file;
	struct errInfo_t*	errInfo;
};

/**
 * Pause or play current file.
 *
 * \return	True if paused.
 */
bool togglePlayback(void);

/**
 * Stops current playback. Playback thread should exit as a result.
 */
void stopPlayback(void);

/**
 * Returns whether music is playing or paused.
 */
bool isPlaying(void);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative and errno set.
 */
int getFileType(const char *file);

/**
 * Should only be called from a new thread only, and have only one playback
 * thread at time. This function has not been written for more than one
 * playback thread in mind.
 *
 * \param	infoIn	Playback information.
 */
void playFile(void* infoIn);

#endif
