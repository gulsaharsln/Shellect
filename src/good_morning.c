#include "good_morning.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

/**
 * Convert minutes from now to HH:MM format for the 'at' command.
 * @param minutes_from_now Minutes from the current time.
 * @param out_time String buffer to store the resulting time in HH:MM format.
 * @param len Length of the out_time buffer.
 */
void minutes_from_now_to_time(int minutes_from_now, char *out_time, size_t len) {
    time_t now = time(NULL);
    struct tm new_time = *localtime(&now);

    new_time.tm_min += minutes_from_now;
    mktime(&new_time); // Normalize the tm structure

    strftime(out_time, len, "%H:%M", &new_time);
}

/**
 * Schedule an audio file to be played after a certain number of minutes.
 * @param minutes Minutes from now when the audio should play.
 * @param audio_path Path to the audio file.
 */
void schedule_audio_playback(int minutes, const char *audio_path) {
    char time_str[6]; // HH:MM
    char command[256];

    // Convert minutes to HH:MM for the 'at' command
    minutes_from_now_to_time(minutes, time_str, sizeof(time_str));

    // Prepare the command to schedule the job using 'at'
    snprintf(command, sizeof(command), 
             "echo 'mpg123 -q \"%s\"' | at %s", audio_path, time_str);

    // Execute the scheduling command
    system(command);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <minutes> <path/to/audio>\n", argv[0]);
        return EXIT_FAILURE;
    }

    GoodMorningConfig config;
    config.minutes = atoi(argv[1]);
    config.audio_path = argv[2];

    if (minutes <= 0) {
        fprintf(stderr, "Invalid number of minutes. Must be greater than 0.\n");
        return EXIT_FAILURE;
    }

    
    schedule_audio_playback(&config);
    printf("Alarm set to play audio in %d minutes.\n", config.minutes);

    return EXIT_SUCCESS;
}
