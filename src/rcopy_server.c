#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "ftree.h"


#ifndef PORT
	#define PORT 53286
#endif

/*
 * rcopy_server main:
 * 
 * 	- PORT number given by Makefile
 *
 */
int main () {

	fcopy_server(PORT);

    return 0;
}
