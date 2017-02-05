#include <3ds.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "flac.h"
#include "mp3.h"
#include "opus.h"
#include "playback.h"
#include "wav.h"
#include "sync.h"

int playFile(const char* file)
{
	struct decoder_fn decoder;
	int16_t*		buffer1 = NULL;
	int16_t*		buffer2 = NULL;
	ndspWaveBuf		waveBuf[2];
	bool			playing = true;
	bool			lastbuf = false;
	int				ret;

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

		default:
			//printf("Unsupported File type.\n");
			return 0;
	}

	if(R_FAILED(ndspInit()))
	{
		//printf("Initialising ndsp failed.");
		goto out;
	}

	if((ret = (*decoder.init)(file)) != 0)
	{
		//printf("Error initialising decoder: %d\n", ret);
		goto out;
	}

	buffer1 = linearAlloc(decoder.buffSize * sizeof(int16_t));
	buffer2 = linearAlloc(decoder.buffSize * sizeof(int16_t));

#ifdef DEBUG
	//printf("\nRate: %lu\tChan: %d\n", (*decoder.rate)(), (*decoder.channels)());
#endif

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

	//printf("Playing %s\n", file);

	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false);

	while(playing == false || ndspChnIsPlaying(CHANNEL) == true)
	{
		/*
		svcWaitSynchronization(event2, U64_MAX);
		svcClearEvent(event2);
		*/
		//if (!svcWaitSynchronization(event1, 1)) break;

		u32 kDown;

		/* Number of bytes read from file.
		 * Static only for the purposes of the printf debug at the bottom.
		 */
		static size_t read = 0;

		hidScanInput();
		kDown = hidKeysDown();

		if(kDown & KEY_B)
			break;

		if(kDown & (KEY_A | KEY_R))
		{
			playing = !playing;
			//printf("\33[2K\r%s", playing == false ? "Paused" : "");
		}

		if(playing == false || lastbuf == true)
			continue;

		if(waveBuf[0].status == NDSP_WBUF_DONE)
		{
			read = (*decoder.decode)(&buffer1[0]);

			if(read == 0)
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
			read = (*decoder.decode)(&buffer2[0]);

			if(read == 0)
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

out:
	//printf("\nStopping playback.\n");
	(*decoder.exit)();
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	linearFree(buffer1);
	linearFree(buffer2);
	return 0;
}

/**
 * Obtains file type.
 *
 * \param	file	File location.
 * \return			File type, else negative.
 */
int getFileType(const char *file)
{
	FILE* ftest = fopen(file, "rb");
	int fileSig = 0;
	enum file_types file_type = FILE_TYPE_ERROR;

	if(ftest == NULL)
	{
		//err_print("Opening file failed.");
		//printf("file: %s\n", file);
		return -1;
	}

	if(fread(&fileSig, 4, 1, ftest) == 0)
	{
		//err_print("Unable to read file.");
		fclose(ftest);
		return -1;
	}

	switch(fileSig)
	{
		// "RIFF"
		case 0x46464952:
			if(fseek(ftest, 4, SEEK_CUR) != 0)
			{
				//err_print("Unable to seek.");
				break;
			}

			// "WAVE"
			// Check required as AVI file format also uses "RIFF".
			if(fread(&fileSig, 4, 1, ftest) == 0)
			{
				//err_print("Unable to read potential WAV file.");
				break;
			}

			if(fileSig != 0x45564157)
				break;

			file_type = FILE_TYPE_WAV;
			//printf("File type is WAV.");
			break;

		// "fLaC"
		case 0x43614c66:
			file_type = FILE_TYPE_FLAC;
			//printf("File type is FLAC.");
			break;

		// "OggS"
		case 0x5367674f:
			if(isOpus(file) == 0)
			{
				//printf("\nFile type is Opus.");
				file_type = FILE_TYPE_OPUS;
			}
			else
			{
				file_type = FILE_TYPE_OGG;
				//printf("\nUnsupported audio in OGG container.");
			}

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
				//puts("File type is MP3.");
				file_type = FILE_TYPE_MP3;
				break;
			}

			//printf("Unknown magic number: %#010x\n.", fileSig);
	}

	fclose(ftest);
	return file_type;
}
