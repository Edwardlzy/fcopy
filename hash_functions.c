#include <stdio.h>
#include "hash.h"
#include <stdlib.h>

// A function that computes the hash value of the given file.
char *hash(FILE *f) {
    char *hash_val = malloc(BLOCK_SIZE);
    if (f == NULL) {
        fprintf(stderr, "%s\n", "Invalid file!");
        exit(1);
    } else {
        for (int i = 0; i < BLOCK_SIZE; i++) {
            hash_val[i] = '\0';
        }
        
        // Read characters and XOR with hash_val one by one.
        char input_char[1];
        int j = 0;
        while (fread(input_char, 1, 1, f) == 1) {
            if (j >= BLOCK_SIZE)
                j = 0;
            hash_val[j] = hash_val[j] ^ input_char[0];
            j++;
        }
    }

    return hash_val;
}
