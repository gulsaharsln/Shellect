#include "dirsize.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

long calculate_directory_size(const char *base_path, int recursive) {
    long total_size = 0;
    DIR *dir = opendir(base_path);

    if (!dir) {
        fprintf(stderr, "Error: Unable to open directory %s\n", base_path);
        return -1; // Return an error code
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *path = malloc(strlen(base_path) + strlen(entry->d_name) + 2);
        sprintf(path, "%s/%s", base_path, entry->d_name);

        struct stat statbuf;
        if (stat(path, &statbuf) != 0) {
            fprintf(stderr, "Error: Unable to access file information %s\n", path);
            free(path);
            closedir(dir);
            return -1; // Return an error code
        }

        if (S_ISDIR(statbuf.st_mode) && recursive) {
            long dir_size = calculate_directory_size(path, recursive);
            if (dir_size == -1) {
                // Pass the error up the call stack
                free(path);
                closedir(dir);
                return -1; // Return an error code
            }
            total_size += dir_size;
        } else if (S_ISREG(statbuf.st_mode)) {
            total_size += statbuf.st_size;
        }

        free(path);
    }

    closedir(dir);
    return total_size;
}

void calculate_dir_size(const DirSizeOptions *options) {
    long total_size = calculate_directory_size(options->path, options->recursive);
    if (total_size == -1) {
        fprintf(stderr, "Error: An error occurred while calculating the directory size.\n");
        exit(EXIT_FAILURE); // Terminate the program
    }

    printf("Total size of directory '%s': %ld bytes\n", options->path, total_size);
}

int main(int argc, char *argv[]) {
    DirSizeOptions options = {.path = ".", .recursive = 0};

    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "r")) != -1) {
        switch (opt) {
            case 'r':
                options.recursive = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-r] [path]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        options.path = argv[optind];
    }

    calculate_dir_size(&options);

    return EXIT_SUCCESS;
}
