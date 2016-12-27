#include <3ds.h>
#include <opus/opusfile.h>

int playOpus(const char* in)
{
	return 0;
}

int isOpus(const char* in)
{
	int err = 0;
	OggOpusFile* opusTest = op_test_file(in, &err);

	op_free(opusTest);
	return err;
}
