#include <errno.h>

/* Channel to play music on */
#define CHANNEL	0x08

/* Adds extra debugging text */
//#define DEBUG

/* Prints more error information */
#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)

struct metadata_t
{
	const char* title;
	const char* album;
	const char* artist;
	const char* album_artist;
	const char* user_comment;
	const char* year;
	const char* track;
	const char* album_number;
	uint8_t popularimeter; // Rating 0-255
	//void* album_cover;
};

struct decoder_fn
{
	int (* init)(const char* file);
	int (* metadata)(struct metadata_t* metadata);
	uint32_t (* rate)(void);
	uint8_t (* channels)(void);
	/* TODO: Remove buffSize */
	int buffSize;
	uint64_t (* decode)(void*);
	void (* exit)(void);
};
