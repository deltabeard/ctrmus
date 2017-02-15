#include <errno.h>
#include <stdio.h>

/* Adds extra debugging text */
//#define DEBUG

/* Prints more error information */
#define err_print(err) \
	do { fprintf(stderr, "\nError %d:%s(): %s %s\n", __LINE__, __func__, \
			err, strerror(errno)); } while (0)

#define delete(ptr) \
	free((void*) ptr); ptr = NULL
