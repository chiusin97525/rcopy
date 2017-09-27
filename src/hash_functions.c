#include <stdio.h>
#include <stdlib.h>


char *hash(FILE *file) {
    char *hash_val = (char *)malloc(8 * sizeof(char*));
    char next_char;
    int counter = 0;
    for(int i = 0; i < 8; i++){
        hash_val[i] = '\0';
    }
    while(fscanf(file, "%c", &next_char) != EOF){
        hash_val[counter] = next_char ^ hash_val[counter];
        counter ++;
        if(counter == 8){
            counter = 0;
        }
    }
    return hash_val;
}
