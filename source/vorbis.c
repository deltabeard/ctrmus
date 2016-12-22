#include <3ds.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <errno.h>
#include <string.h>
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>

#define CHANNEL			0x08
#define BUFFER_SIZE		2 * 1024 * 1024

int playOgg(const char* file)
{
	OggVorbis_File	vf;
	int				current_section;
	FILE*			f;
	int				rate;
	int				channels;
	s16*			buffer1 = NULL;
	s16*			buffer2 = NULL;
	char			pcmout[2048];
	ndspWaveBuf		waveBuf[2];
	bool			playing = true;
	bool			lastbuf = false;
	long			read = 0;
	int				i = 0;

	if(R_FAILED(ndspInit()))
	{
		printf("Initialising ndsp failed.");
		return -1;
	}

	f = fopen(file, "r");

	if(file == NULL)
	{
		printf("Opening file failed.");
		return -1;
	}

	if(ov_open(f, &vf, NULL, 0) < 0)
	{
		fprintf(stderr,"Input does not appear to be an Ogg bitstream.\n");
		return -1;
	}

	/* Throw the comments plus a few lines about the bitstream we're
	   decoding */
	{
		char **ptr = ov_comment(&vf, -1)->user_comments;
		vorbis_info *vi = ov_info(&vf, -1);

		if(vi->channels > 2)
		{
			printf("Unsupported number of channels.");
			goto out;
		}
		ndspChnReset(CHANNEL);
		ndspChnWaveBufClear(CHANNEL);
		ndspChnSetRate(CHANNEL, vi->rate);
		ndspChnSetFormat(CHANNEL, vi->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

		while(*ptr)
		{
			fprintf(stderr, "%s\n", *ptr);
			++ptr;
		}

		fprintf(stderr,"\nBitstream is %d channel, %ldHz\n",vi->channels,vi->rate);
		fprintf(stderr,"\nDecoded length: %ld samples\n",
				(long)ov_pcm_total(&vf,-1));
		fprintf(stderr,"Encoded by: %s\n\n",ov_comment(&vf,-1)->vendor);
	}

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	/* Polyphase sounds much better than linear or no interpolation */
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_POLYPHASE);
	memset(waveBuf, 0, sizeof(waveBuf));

	printf("Char: %d, s16: %d\n", sizeof(char), sizeof(s16));
	buffer1 = (s16*) linearAlloc(BUFFER_SIZE);
	//buffer2 = (s16*) linearAlloc(BUFFER_SIZE);
	while(i < BUFFER_SIZE - 2048)
	{
		read += ov_read(&vf, pcmout, sizeof(pcmout), &current_section);
		i += read / 2;
		memcpy(&buffer1[i], pcmout, read);
	}
	waveBuf[0].nsamples = BUFFER_SIZE / 4;
	waveBuf[0].data_vaddr = &buffer1[0];
	ndspChnWaveBufAdd(CHANNEL, &waveBuf[0]);
	/**
	 * There may be a chance that the music has not started by the time we get
	 * to the while loop. So we ensure that music has started here.
	 */
	while(ndspChnIsPlaying(CHANNEL) == false)
	{}

	while(ndspChnIsPlaying(CHANNEL) == true)
	{}

out:
	ndspChnWaveBufClear(CHANNEL);
	ov_clear(&vf);
	fclose(f);
	linearFree(buffer1);
	ndspExit();
  	fprintf(stderr,"Done.\n");
	return 0;
}
