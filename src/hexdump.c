#include "hexdump.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

void hexdump(const HexdumpConfig *config) {
    int fd = STDIN_FILENO;
    if (config->filename) {
        fd = open(config->filename, O_RDONLY);
        if (fd == -1) {
            perror("open");
            return;
        }
    }

    unsigned char buffer[16];
    ssize_t bytes_read;
    unsigned long offset = 0;

    // Loop to read and print the file content in hexadecimal format
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        printf("%08lx: ", offset);
        for (int i = 0; i < bytes_read; ++i) {
            if (i > 0 && i % config->group_size == 0) printf(" ");
            printf("%02x", buffer[i]);
        }
        printf("\n");
        offset += bytes_read;
    }

    if (fd != STDIN_FILENO) close(fd);
}

int main(int argc, char *argv[]) {
    HexdumpConfig config;
    config.group_size = 1; // Default group size
    config.filename = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "g:")) != -1) {
        switch (opt) {
            case 'g':
                config.group_size = atoi(optarg);
                if (config.group_size <= 0 || config.group_size > 16 || (config.group_size & (config.group_size - 1)) != 0) {
                    fprintf(stderr, "Invalid group size. Must be a power of 2 and not larger than 16.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-g group_size] [file]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        config.filename = argv[optind];
    }

    hexdump(&config);
    return 0;
}
