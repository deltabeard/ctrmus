#include <errno.h>

/* Channel to play music on */
#define CHANNEL	0x08

/* Adds extra debugging text */
//#define DEBUG

/* Prints more error information */
#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)

#define delete(ptr) \
	free(ptr); ptr = NULL

struct decoder_fn
{
	int (* init)(const char* file);
	uint32_t (* rate)(void);
	uint8_t (* channels)(void);
	int buffSize;
	uint64_t (* decode)(void*);
	void (* exit)(void);
};
