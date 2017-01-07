#include <opus/opusfile.h>

void setOpus(struct decoder_fn* decoder);

int initOpus(const char* file);

uint32_t rateOpus(void);

uint64_t decodeOpus(void* buffer);

void exitOpus(void);

int playOpus(const char* in);

uint64_t fillOpusBuffer(OggOpusFile* opusFile, uint64_t samplesToRead,
		int16_t* bufferOut);

int isOpus(const char* in);
