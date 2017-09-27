#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <libgen.h>
#include <fcntl.h>

// Header Files
#include "ftree.h"
#include "hash.h"

// Server/Client related libraries
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Function Prototype
int transmit_file(int sock_fd, char *src_path, size_t size);
int file_copy(int sock_fd, struct fileinfo income_file);
int file_check(struct fileinfo income_file);
int client_to_server(char *src_path, char *dest_path, int sock_fd);

/* Server */
void fcopy_server(int port) {
	struct fileinfo income_file;

	
    // Create socket
	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd < 0) {
		perror("server: socket");
		exit(1);
	}

	// Bind socket to an address
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;
	memset(&server.sin_zero, 0, 8);

	//printf("Binding Server...\n");
	if (bind(sock_fd, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) < 0) {
		perror("server: bind");
		close(sock_fd);
		exit(1);
	}

	//printf("Listening to requests...\n");

	// Create queue to new connection requests
	if(listen(sock_fd, 5) < 0) {
		perror("server: listen");
		close(sock_fd);
		exit(1);
	}
	
	// keep waiting for new connection
	while(1){
	int client_fd = accept(sock_fd, NULL, NULL);
	int result;
		// keep getting info from the same connection
		while(client_fd)
		{
			if(client_fd < 0) {
				perror("server: accept");
				close(sock_fd);
				exit(1);
			}
			
			// get info for the fileinfo struct
			if(read(client_fd, income_file.path, MAXPATH) < 1){
				break;
			}
			read(client_fd, &(income_file.mode), sizeof(income_file.mode));
			// if the file being transfer is a file, get its hash value
			if(S_ISREG(income_file.mode)){
				read(client_fd, income_file.hash, HASH_SIZE);
			}
			read(client_fd, &(income_file.size), sizeof(income_file.size));
		
			// check the state of the file
			result = file_check(income_file);
			// send respond to client
			write(client_fd, &result, sizeof(int));
			
			// if file MISMATCH, get the file from client to perform a copy
			if(result == MISMATCH){
				//printf("MISMATCH, copying\n");
				result = file_copy(client_fd, income_file);
				printf("This ran, result is %d\n", result);
				if(result != TRANSMIT_OK){
					printf("Error, transmit failed\n");
				}
			}
			
			
		}
		close(client_fd);
	}
	close(sock_fd);
}

/*
	Copy the file, return TRANSMIT_OK on success, TRANSMIT_ERROR otherwise
*/
int file_copy(int sock_fd, struct fileinfo income_file){
	char buffer[1024*1024];
	FILE * dest_file = fopen(income_file.path, "w");
	
    if(dest_file == NULL){
        perror("Error");
        return TRANSMIT_ERROR;
	}
	
	read(sock_fd, buffer, sizeof(buffer));
	//fprintf(dest_file, "%s", buffer);
	//printf("%s\n", buffer);
	fwrite(buffer, 1, income_file.size, dest_file);
	
	fclose(dest_file);
	
	return TRANSMIT_OK;
}


/*
	Find the file in the server, return the appropiate value to indicate:
	MATCH, MISMATCH, MATCH_ERROR: for error
*/
int file_check(struct fileinfo income_file){
	struct stat dest_file_stat;
	
	// check if there is such file on the server
	// if file is not found, return MISMATCH
	if(lstat(income_file.path, &dest_file_stat) < 0){
		if (S_ISDIR(income_file.mode)){
			// create the directory
			mkdir(income_file.path, (income_file.mode & (S_IRWXU | S_IRWXG | S_IRWXO)));
		}
		else if (S_ISREG(income_file.mode)){
			return MISMATCH;
		}
    }
	// if a file is found
	else if(S_ISREG(income_file.mode)){
		// check for file type, if differ return MATCH_ERROR
		if(S_ISREG(income_file.mode) && S_ISDIR(dest_file_stat.st_mode)){
			
			return MATCH_ERROR;
		}
		else{
			// check for the size, if differ return MISMATCH
			if(income_file.size != dest_file_stat.st_size){
				return MISMATCH;
			}
			// if size is same then check for hash, if hash differ return MISMATCH
			else{
				FILE *dest_file = fopen(income_file.path, "r");
				char *dest_hash = hash(dest_file);
				fclose(dest_file);
				if(strcmp(income_file.hash ,dest_hash) != 0){
					return MISMATCH;
				}
			}
		}
	}else if(S_ISDIR(income_file.mode)){
	
		if(S_ISDIR(income_file.mode) && S_ISREG(dest_file_stat.st_mode)){
			return MATCH_ERROR;
		}
	}
	
	// update the permission
	chmod(income_file.path, income_file.mode & (S_IRWXU | S_IRWXG | S_IRWXO));
	// default return MATCH
	return MATCH;
}



/* Client */
int fcopy_client(char *src_path, char *dest_path, char *host, int port) {
	
	int soc;
    struct sockaddr_in peer;

  
  
	// creating the connection'
	if ((soc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("rcopy_client: socket");
		exit(1);
	}

	peer.sin_family = AF_INET;
	peer.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &peer.sin_addr) < 1) {
		perror("rcopy_client: inet_pton");
		close(soc);
		exit(1);
	}

	if (connect(soc, (struct sockaddr *)&peer, sizeof(peer)) == -1) {
		perror("rcopy_client: connect");
		exit(1);
	}
	

	int result = client_to_server(src_path, dest_path, soc);
	
	close(soc);
	if(result == MATCH_ERROR){
		return 1;
	}
	
	return 0;
}


/* 
	Handle transmition between client and server.
*/
int client_to_server(char *src_path, char *dest_path, int sock_fd){
	struct stat src_stat;
	int respond = 0;
	
	// Get the info for the src: mode, hash, size
	if(lstat(src_path, &src_stat) < 0){
        perror("Error");
        return -1;
    }
	
	// get the file name
	char *file_name = malloc(strlen(src_path)*sizeof(char *));
    strcpy(file_name, src_path);
    file_name = basename(file_name);
	// string representation of the path to src
	char *new_src_path = malloc(strlen(src_path)*sizeof(char *)+2);
	strcpy(new_src_path, src_path);
	strcat(new_src_path, "/");
	// string rep of path to dest
	char *new_dest_path = malloc(strlen(dest_path)*sizeof(char *)+2);
	strcpy(new_dest_path, dest_path);
	strcat(new_dest_path, "/");
	strcat(new_dest_path, file_name);

	// if the src is a file, simply check with server
	if(S_ISREG(src_stat.st_mode)){
		
		// open the file
		FILE * src_file = fopen(src_path, "r");
		
		if(src_file == NULL){
			perror("Error");
        return -1;
		}
		// compute the hash value
		char *src_hash_val = hash(src_file);
		
        // send the info to the server
		write(sock_fd, new_dest_path, MAXPATH);
		write(sock_fd, &(src_stat.st_mode), sizeof(src_stat.st_mode));
		write(sock_fd, src_hash_val, HASH_SIZE);
		write(sock_fd, &(src_stat.st_size), sizeof(src_stat.st_size));
		read(sock_fd, &respond, sizeof(int));
		
		fclose(src_file);
		
		// if mismatch, send file
		if(respond == MISMATCH){
			int second_respond = transmit_file(sock_fd, src_path, src_stat.st_size);
			if(second_respond == TRANSMIT_ERROR){
				fprintf(stderr, "File transmit failed at location: %s\n", src_path);
				return second_respond;
			}
		}
		else if(respond == MATCH_ERROR){
			fprintf(stderr, "Incorrect type of file at location: %s\n", src_path);
			return respond;
		}
		
    }
	
	// if the src is a directory, recursively go through the directory
	else if(S_ISDIR(src_stat.st_mode)){
		
		// send the info to the server
		write(sock_fd, new_dest_path, MAXPATH);
		write(sock_fd, &(src_stat.st_mode), sizeof(src_stat.st_mode));
		write(sock_fd, &(src_stat.st_size), sizeof(src_stat.st_size));
		read(sock_fd, &respond, sizeof(int));
		// check respond from server, if match error, print the causing file name and terminate
		if(respond == MATCH_ERROR){
			fprintf(stderr, "Incorrect type of file at location: %s\n", src_path);
			return respond;
		}
		
		// Open the directory
		DIR *src_dir = opendir(src_path);
		struct dirent *next_file;
		
		if(src_dir == NULL){
			perror("Error");
			return -1;
		}
		// loop through all the content in directory.
		while((next_file = readdir(src_dir))){
			int empty = 0;
			// find the first content that does not start with '.'
			while(next_file->d_name[0] == '.'){
				next_file = readdir(src_dir);
				if(next_file == NULL){
					empty = 1;
					break;
				}
			}
			
			// exit the loop if the directory is empty
			if(empty == 1){
				break;  
			}
			
			// create a sting for the next source file path
			char *next_src_path = malloc((strlen(next_file->d_name)+strlen(new_src_path))*sizeof(char *)+1);
			strcpy(next_src_path, new_src_path);
			strcat(next_src_path, next_file->d_name);

			
			// recursively go check with the server
			respond = client_to_server(next_src_path, new_dest_path, sock_fd);
			if(respond == MATCH_ERROR){
				perror("Error: %s\n");
				return respond;
			}
			
			free(next_src_path);
		}
		closedir(src_dir);
	}
	
	free(new_src_path);
	free(new_dest_path);
	return respond;
}

/*
	Transfer the file content to the server.
*/
int transmit_file(int sock_fd, char *src_path, size_t size){
	char buffer[1024*1024];
	int input_file = open(src_path, O_RDONLY);
	
	while (1) {
    // Read data into buffer.  We may not have enough to fill up buffer, so we
    // store how many bytes were actually read in bytes_read.
    int bytes_read = read(input_file, buffer, sizeof(buffer));
    if (bytes_read == 0) // We're done reading from the file
        break;

    if (bytes_read < 0) {
        // handle errors
		break;
    }

    // You need a loop for the write, because not all of the data may be written
    // in one call; write will return how many bytes were written. p keeps
    // track of where in the buffer we are, while we decrement bytes_read
    // to keep track of how many bytes are left to write.
    void *p = buffer;
    while (bytes_read > 0) {
        int bytes_written = write(sock_fd, p, bytes_read);
        if (bytes_written <= 0) {
            // handle errors
			break;
        }
        bytes_read -= bytes_written;
        p += bytes_written;
		}
	}
	//printf("Buffer closed\n");
	close(input_file);
	return 0;
}
