#include <stddef.h>
#ifndef GOOD_MORNING_H
#define GOOD_MORNING_H

typedef struct {
    int minutes;          // Minutes from now when the alarm should go off
    const char *audio_path; // Path to the audio file that should be played
} GoodMorningConfig;

void minutes_from_now_to_time(int minutes_from_now, char *out_time, size_t len);
void schedule_audio_playback(const GoodMorningConfig *config);

#endif // GOOD_MORNING_H
