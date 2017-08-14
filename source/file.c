#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "error.h"
#include "file.h"
#include "flac.h"
#include "mp3.h"
#include "opus.h"
#include "vorbis.h"
#include "wav.h"

/**
 * Obtain file type string from file_types enum.
 *
 * \param	ft	File type enum.
 * \return		String representation of enum.
 */
const char* fileToStr(enum file_types ft)
{
	static const char *file_types_str[] = {
		"UNKNOWN",
		"WAV",
		"FLAC",
		"VORBIS",
		"OPUS",
		"MP3"
	};

	return file_types_str[ft];
}

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			file_types enum or 0 on error and errno set.
 */
enum file_types getFileType(const char *file)
{
	FILE* ftest = fopen(file, "rb");
	uint32_t fileSig;
	enum file_types file_type = FILE_TYPE_ERROR;

	/* Failure opening file */
	if(ftest == NULL)
		return -1;

	if(fread(&fileSig, 4, 1, ftest) == 0)
		goto err;

	switch(fileSig)
	{
		// "RIFF"
		case 0x46464952:
			if(fseek(ftest, 4, SEEK_CUR) != 0)
				break;

			// "WAVE"
			// Check required as AVI file format also uses "RIFF".
			if(fread(&fileSig, 4, 1, ftest) == 0)
				break;

			if(fileSig != 0x45564157)
				break;

			file_type = FILE_TYPE_WAV;
			break;

		// "fLaC"
		case 0x43614c66:
			file_type = FILE_TYPE_FLAC;
			break;

		// "OggS"
		case 0x5367674F:
			if(isOpus(file) == 0)
				file_type = FILE_TYPE_OPUS;
			else if(isFlac(file) == 0)
				file_type = FILE_TYPE_FLAC;
			else if(isVorbis(file) == 0)
				file_type = FILE_TYPE_VORBIS;
			else
				errno = FILE_NOT_SUPPORTED;

			break;

		default:
			/*
			 * MP3 without ID3 tag, ID3v1 tag is at the end of file, or MP3
			 * with ID3 tag at the beginning  of the file.
			 */
			if((fileSig << 16) == 0xFBFF0000 ||
					(fileSig << 16) == 0xFAFF0000 ||
					(fileSig << 8) == 0x33444900)
			{
				file_type = FILE_TYPE_MP3;
				break;
			}

			/* TODO: Add this again at some point */
			//printf("Unknown magic number: %#010x\n.", fileSig);
			errno = FILE_NOT_SUPPORTED;
			break;
	}

err:
	fclose(ftest);
	return file_type;
}

