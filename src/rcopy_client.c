#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ftree.h"

#ifndef PORT
	#define PORT 53286
#endif

/*
 * rcopy_client main
 */
int main(int argc, char **argv) {

	// Checks format of IP and number of arguments
    if(argc != 4) {
        perror("client: Incorrect number of args");
        return -1;
    }

    fcopy_client(argv[1], argv[2], argv[3], PORT);


    return 0;
}
