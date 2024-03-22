#ifndef DIRSIZE_H
#define DIRSIZE_H


typedef struct {
    const char *path;   // Path to the directory
    int recursive;      // Flag to indicate if subdirectories should be included
} DirSizeOptions;

/**
 * Calculates the total size of files in a directory based on the given options.
 * @param options DirSizeOptions containing path and recursive flag.
 */
void calculate_dir_size(const DirSizeOptions *options);

#endif // DIRSIZE_H
