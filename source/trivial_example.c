/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE libopusfile SOFTWARE CODEC SOURCE CODE. *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE libopusfile SOURCE CODE IS (C) COPYRIGHT 1994-2012           *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*For fileno()*/
#if !defined(_POSIX_SOURCE)
# define _POSIX_SOURCE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opus/opusfile.h>
#if defined(_WIN32)
# include "win32utf8.h"
# undef fileno
# define fileno _fileno
#endif

static void put_le32(unsigned char *_dst,opus_uint32 _x){
	_dst[0]=(unsigned char)(_x&0xFF);
	_dst[1]=(unsigned char)(_x>>8&0xFF);
	_dst[2]=(unsigned char)(_x>>16&0xFF);
	_dst[3]=(unsigned char)(_x>>24&0xFF);
}

/*Make a header for a 48 kHz, stereo, signed, 16-bit little-endian PCM WAV.*/
static void make_wav_header(unsigned char _dst[44],ogg_int64_t _duration){
	/*The chunk sizes are set to 0x7FFFFFFF by default.
	  Many, though not all, programs will interpret this to mean the duration is
	  "undefined", and continue to read from the file so long as there is actual
	  data.*/
	static const unsigned char WAV_HEADER_TEMPLATE[44]={
		'R','I','F','F',0xFF,0xFF,0xFF,0x7F,
		'W','A','V','E','f','m','t',' ',
		0x10,0x00,0x00,0x00,0x01,0x00,0x02,0x00,
		0x80,0xBB,0x00,0x00,0x00,0xEE,0x02,0x00,
		0x04,0x00,0x10,0x00,'d','a','t','a',
		0xFF,0xFF,0xFF,0x7F
	};
	memcpy(_dst,WAV_HEADER_TEMPLATE,sizeof(WAV_HEADER_TEMPLATE));
	if(_duration>0){
		if(_duration>0x1FFFFFF6){
			fprintf(stderr,"WARNING: WAV output would be larger than 2 GB.\n");
			fprintf(stderr,
					"Writing non-standard WAV header with invalid chunk sizes.\n");
		}
		else{
			opus_uint32 audio_size;
			audio_size=(opus_uint32)(_duration*4);
			put_le32(_dst+4,audio_size+36);
			put_le32(_dst+40,audio_size);
		}
	}
}

int convOpus(const char* in, const char *outf){
	OggOpusFile		*of;
	FILE			*wav;
	ogg_int64_t		duration;
	unsigned char	wav_header[44];
	int				ret;
	int				output_seekable;

	of = op_open_file(in, &ret);

	if((wav = fopen(outf, "w+")) == NULL)
	{
		fprintf(stderr,"Failed to open file '%s': %i\n",outf,errno);
		return EXIT_FAILURE;
	}

	if(of==NULL){
		fprintf(stderr,"Failed to open file '%s': %i\n",in,ret);
		return EXIT_FAILURE;
	}

	duration=0;
	output_seekable=fseek(wav,0,SEEK_CUR)!=-1;

	if(op_seekable(of)){
		fprintf(stderr,"Total number of links: %i\n",op_link_count(of));
		duration=op_pcm_total(of,-1);
		fprintf(stderr,"Total duration: ");
		fprintf(stderr," (%li samples @ 48 kHz)\n",(long)duration);
	}
	else if(!output_seekable){
		fprintf(stderr,"WARNING: Neither input nor output are seekable.\n");
		fprintf(stderr,
				"Writing non-standard WAV header with invalid chunk sizes.\n");
	}

	make_wav_header(wav_header,duration);

	if(!fwrite(wav_header,sizeof(wav_header),1,wav)){
		fprintf(stderr,"Error writing WAV header: %s\n",strerror(errno));
		ret=EXIT_FAILURE;
	}
	else{
		ogg_int64_t nsamples;
		opus_int16*		pcm = malloc(120*48*2*sizeof(opus_int16));
		unsigned char*	out = malloc(120*48*2*2*sizeof(unsigned char));
		nsamples=0;
		for(;;){
			int           si;
			/*Although we would generally prefer to use the float interface, WAV
			  files with signed, 16-bit little-endian samples are far more
			  universally supported, so that's what we output.*/
			ret=op_read_stereo(of,pcm,sizeof(pcm)/sizeof(*pcm));
			if(ret<0){
				fprintf(stderr,"\nError decoding '%s': %i\n",in,ret);
				ret=EXIT_FAILURE;
				break;
			}
			if(ret<=0){
				ret=EXIT_SUCCESS;
				break;
			}
			/*Ensure the data is little-endian before writing it out.*/
			for(si=0;si<2*ret;si++){
				out[2*si+0]=(unsigned char)(pcm[si]&0xFF);
				out[2*si+1]=(unsigned char)(pcm[si]>>8&0xFF);
			}
			if(!fwrite(out,sizeof(*out)*4*ret,1,wav)){
				fprintf(stderr,"\nError writing decoded audio data: %s\n",
						strerror(errno));
				ret=EXIT_FAILURE;
				break;
			}
			nsamples+=ret;

			if(nsamples % 1000 == 0)
				fprintf(stderr, "\rSample: %li", (long)nsamples);
			//End of for loop.
		}
		free(pcm);
		free(out);
		if(ret==EXIT_SUCCESS){
			fprintf(stderr,"\nDone: played ");
			fprintf(stderr," (%li samples @ 48 kHz).\n",(long)nsamples);
		}
		if(op_seekable(of)&&nsamples!=duration){
			fprintf(stderr,"\nWARNING: "
					"Number of output samples does not match declared file duration.\n");
			if(!output_seekable)fprintf(stderr,"Output WAV file will be corrupt.\n");
		}
		if(output_seekable&&nsamples!=duration){
			make_wav_header(wav_header,nsamples);
			if(fseek(wav,0,SEEK_SET)||
					!fwrite(wav_header,sizeof(wav_header),1,wav)){
				fprintf(stderr,"Error rewriting WAV header: %s\n",strerror(errno));
				ret=EXIT_FAILURE;
			}
		}
	}

	fclose(wav);
	op_free(of);
	return ret;
}


