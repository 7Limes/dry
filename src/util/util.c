#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>


int file_exists(char* path) {
    struct stat buffer;
    return stat(path, &buffer) == 0;
}


// Based on https://stackoverflow.com/a/3464656
// Reads data from `file_path` into `output_buffer` and stores the length in `length`.
int read_file_bytes(uint8_t **output_buffer, size_t *length, const char *file_path) {
    if (!output_buffer || !file_path) {
        return -1;  // Invalid arguments
    }

    FILE *handler = fopen(file_path, "rb");
    if (!handler) {
        return -2;  // File cannot be opened
    }
    
    if (fseek(handler, 0, SEEK_END) != 0) {
        fclose(handler);
        return -3;  // Seek error
    }

    long file_size = ftell(handler);
    if (file_size < 0) {
        fclose(handler);
        return -4;  // Tell error
    }
    rewind(handler);

    uint8_t *buffer = malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(handler);
        return -5;  // Memory allocation failure
    }

    size_t read_size = fread(buffer, sizeof (uint8_t), (size_t) file_size, handler);
    if (read_size != (size_t) file_size) {
        free(buffer);
        fclose(handler);
        return -6;  // Read error
    }

    buffer[file_size] = '\0';
    fclose(handler);

    *output_buffer = buffer;

    if (length != NULL) {
        *length = (size_t) file_size;
    }

    return 0;  // Success
}
