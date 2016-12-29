#include <opus/opusfile.h>

int playOpus(const char* in);

uint64_t fillOpusBuffer(OggOpusFile* opusFile, uint64_t samplesToRead,
		int16_t* bufferOut);

int isOpus(const char* in);
