enum file_types
{
	FILE_TYPE_ERROR = -1,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_OGG,
	FILE_TYPE_OPUS,
	FILE_TYPE_MP3
};

struct playbackInfo_t
{
	char*				file;
	struct errInfo_t*	errInfo;
};

void playFile(void* infoIn);

/**
 * Pause or play current file.
 *
 * \return	True if paused.
 */
bool togglePlayback(void);

void stopPlayback(void);

bool isPlaying(void);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file);
