#include <3ds.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <stdlib.h>
#include <string.h>

#include "all.h"
#include "ffmpeg.h"

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *audio_dec_ctx;
static AVStream *audio_stream = NULL;
static int audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket pkt;


/* Enable or disable frame reference counting. You are not supposed to support
 * both paths in your application but pick the one most appropriate to your
 * needs. Look for the use of refcount in this example to see what are the
 * differences of API usage between them. */
static int refcount = 0;

static int decode_packet(int *got_frame, int cached, int16_t* bufferOut,
		uint64_t* samples)
{
	int ret = 0;
	int decoded = pkt.size;
	*got_frame = 0;
	//int16_t* bufferOut = (int16_t) buffer;

	if (pkt.stream_index == audio_stream_idx)
	{
		/* decode audio frame */
		ret = avcodec_decode_audio4(audio_dec_ctx, frame, got_frame, &pkt);

		if (ret < 0)
		{
			fprintf(stderr, "Error decoding audio frame (%s)\n", av_err2str(ret));
			return ret;
		}

		/* Some audio decoders decode only part of the packet, and have to be
		 * called again with the remainder of the packet data.
		 * Sample: fate-suite/lossless-audio/luckynight-partial.shn
		 * Also, some decoders might over-read the packet. */
		decoded = FFMIN(ret, pkt.size);

		if(*got_frame)
		{
			size_t unpadded_linesize = frame->nb_samples * av_get_bytes_per_sample(frame->format);

			/* 
			 * nb_s = number of samples decoded
			 * pts = seconds
			 * s = size of buffer required
			 */
			printf("\33[2K\raf%s nb_s:%d pts:%s s:%d dec:%d c:%d",
					cached ? "(cached)" : "",
					frame->nb_samples,
					av_ts2timestr(frame->pts, &audio_dec_ctx->time_base),
					unpadded_linesize,
					decoded,
					channelffmpeg());

			*samples += unpadded_linesize * channelffmpeg();

			/* Write the raw audio data samples of the first plane. This works
			 * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
			 * most audio decoders output planar audio, which uses a separate
			 * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
			 * In other words, this code will write only the first audio channel
			 * in these cases.
			 * You should use libswresample or libavfilter to convert the frame
			 * to packed data. */
			//fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
			memcpy(bufferOut, frame->extended_data[0], unpadded_linesize * channelffmpeg());

#if 0
			for(int i = 0; i < unpadded_linesize; i++)
			{
				*bufferOut = frame->extended_data[0][i];

				if(audio_dec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP)
					*bufferOut = (int16_t)(*bufferOut * 32767.0f);

				bufferOut++;
			}
#endif
		}
	}

	/* If we use frame reference counting, we own the data and need
	 * to de-reference it when we don't use it anymore */
	if (*got_frame && refcount)
		av_frame_unref(frame);

	return decoded;
}

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
		AVFormatContext *fmt_ctx, enum AVMediaType type, const char* file)
{
	int ret, stream_index;
	AVStream *st;
	AVCodec *dec = NULL;
	AVDictionary *opts = NULL;
	ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
				av_get_media_type_string(type), file);
		return ret;
	} else {
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];
		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n",
					av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}
		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx) {
			fprintf(stderr, "Failed to allocate the %s codec context\n",
					av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}
		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
			fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
					av_get_media_type_string(type));
			return ret;
		}
		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
		if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n",
					av_get_media_type_string(type));
			return ret;
		}
		*stream_idx = stream_index;
	}
	return 0;
}

/**
 * Initialise FFMPEG.
 *
 * \param	file	Location of file to play.
 * \return			0 on success, else failure.
 */
int initffmpeg(const char* file)
{
	int ret;
	av_log_set_level(AV_LOG_INFO);

	/* Initialise all formats and codecs */
	av_register_all();

	/* open input file, and allocate format context */
	if((ret = avformat_open_input(&fmt_ctx, file, NULL, NULL)) < 0)
	{
		fprintf(stderr, "Could not open source file %s. Err: %s\n", file, av_err2str(ret));
		return -1;
	}

	/* retrieve stream information */
	if(avformat_find_stream_info(fmt_ctx, NULL) < 0)
	{
		fprintf(stderr, "Could not find stream information\n");
		return -1;
	}

	if(open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO, file) >= 0)
		audio_stream = fmt_ctx->streams[audio_stream_idx];

	/* dump input information to stderr */
	av_dump_format(fmt_ctx, 0, file, 0);

	if(!audio_stream)
	{
		fprintf(stderr, "Could not find audio stream in the input, aborting\n");
		return -1;
	}

	frame = av_frame_alloc();

	if(!frame)
	{
		fprintf(stderr, "Could not allocate frame, err: %d.\n", AVERROR(ENOMEM));
		return -1;
	}

	/* initialize packet, set data to NULL, let the demuxer fill it */
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	av_log_set_level(AV_LOG_PANIC);

	return 0;
}

/**
 * Get sampling rate of file.
 *
 * \return	Sampling rate.
 */
uint32_t rateffmpeg(void)
{
	return audio_dec_ctx->sample_rate;
}

/**
 * Get number of channels of Opus file.
 *
 * \return	Number of channels for opened file.
 */
uint8_t channelffmpeg(void)
{
	enum AVSampleFormat sfmt = audio_dec_ctx->sample_fmt;

	printf("fmt:%s", av_get_sample_fmt_name(sfmt));

	if(av_sample_fmt_is_planar(sfmt))
	{
		//const char *packed = av_get_sample_fmt_name(sfmt);
		//printf("Warning: the sample format the decoder produced is planar "
		//		"(%s). This example will output the first channel only.\n",
		//		packed ? packed : "?");
		sfmt = av_get_packed_sample_fmt(sfmt);
		return 1;
	}

	return audio_dec_ctx->channels;
}

/**
 * Decode part of open Opus file.
 *
 * \param buffer	Decoded output.
 * \return			Samples read for each channel.
 */
uint64_t decodeffmpeg(void* buffer)
{
	uint64_t samples = 0;

	/* read frames from the file */
	while(av_read_frame(fmt_ctx, &pkt) >= 0 && samples < 64 * 1024)
	{
		AVPacket orig_pkt = pkt;
		int got_frame;

		do
		{
			int ret = decode_packet(&got_frame, 0, buffer + samples, &samples);
			if(ret < 0)
				break;

			pkt.data += ret;
			pkt.size -= ret;
		} while(pkt.size > 0);

		av_packet_unref(&orig_pkt);
	}

	return samples;
}

/**
 * Free ffmpeg decoder.
 */
void exitffmpeg(void)
{
	avcodec_free_context(&audio_dec_ctx);
	avformat_close_input(&fmt_ctx);
	av_frame_free(&frame);
}
