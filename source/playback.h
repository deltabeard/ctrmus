enum file_types
{
	FILE_TYPE_ERROR = -1,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_OGG,
	FILE_TYPE_OPUS,
	FILE_TYPE_MP3
};

/**
 * Linked list storing files in queue.
 */
typedef struct qitem
{
	const char*		file;
	struct qitem*	next;
} qitem_t;

/**
 * Play linked list in order.
 */
void playQueue(void *arg);

/**
 * Adds a file to queue.
 *
 * \param	file	Absolute path of file.
 * \return			0 on success, else failure.
 */
int addToQueue(const char* file);

/**
 * Initialise playback module.
 */
void initPlayback(void);

/**
 * Free playback module.
 */
void exitPlayback(void);

int playFile(const char* file);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file);
