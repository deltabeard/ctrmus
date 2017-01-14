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
 * Adds a file to queue.
 *
 * \param	file	Absolute path of file.
 * \return			0 on success, else failure.
 */
int addToQueue(const char* file);

int playFile(const char* file);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file);
