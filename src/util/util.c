#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>


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


/*
 * Converts a string to an int. Supports:
 *   - Optional leading whitespace
 *   - Optional '+' or '-' sign
 *   - Hexadecimal (0x/0X prefix) or decimal digits
 *
 * Returns 1 on success, 0 on failure
 */
int str_to_int(const char *str, int *out) {
    if (str == NULL || out == NULL) {
        return 0;
    }

    const char *p = str;

    // Skip leading whitespace
    while (isspace((unsigned char)*p)) {
        p++;
    }

    // Handle sign
    int negative = 0;
    if (*p == '-' || *p == '+') {
        negative = (*p == '-');
        p++;
    }

    // Detect base: hex if prefixed with 0x/0X, otherwise decimal
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    }

    if (*p == '\0') {
        return 0; // nothing after sign/prefix
    }

    char *endptr;
    long value = strtol(p, &endptr, base);

    if (endptr == p) {
        return 0; // no valid digits found
    }

    // Skip trailing whitespace
    while (isspace((unsigned char)*endptr)) {
        endptr++;
    }

    if (*endptr != '\0') {
        return 0; // leftover garbage characters
    }

    if (negative) {
        value = -value;
    }

    if (value > INT_MAX || value < INT_MIN) {
        return 0; // overflow
    }

    *out = (int)value;
    return 1;
}
