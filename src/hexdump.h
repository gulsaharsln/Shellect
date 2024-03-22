#ifndef HEXDUMP_H
#define HEXDUMP_H


typedef struct {
    int group_size;       // The number of bytes to group together in the output
    const char *filename; // The name of the file to be dumped. If NULL, reads from STDIN.
} HexdumpConfig;

void hexdump(const HexdumpConfig *config);

#endif // HEXDUMP_H
