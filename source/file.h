enum file_types
{
	FILE_TYPE_ERROR = 0,
	FILE_TYPE_WAV,
	FILE_TYPE_FLAC,
	FILE_TYPE_VORBIS,
	FILE_TYPE_OPUS,
	FILE_TYPE_MP3
};

/**
 * Obtain file type string from file_types enum.
 *
 * \param	ft	File type enum.
 * \return		String representation of enum.
 */
const char* fileToStr(enum file_types ft);

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			file_types enum or 0 on error and errno set.
 */
enum file_types getFileType(const char *file);
