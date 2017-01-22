#include <errno.h>

/* Channel to play music on */
#define CHANNEL	0x08

/* Adds extra debugging text */
//#define DEBUG

/* Prints more error information */
#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)
