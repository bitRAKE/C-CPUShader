#include "frame_timing.h"

#define FRAME_TIME_HISTORY_COUNT 64

static double g_frame_times[FRAME_TIME_HISTORY_COUNT];
static double g_frame_time_sum = 0.0;
static int    g_frame_time_index = 0;
static int    g_frame_time_count = 0;

void frame_timing_reset(void)
{
    g_frame_time_sum = 0.0;
    g_frame_time_index = 0;
    g_frame_time_count = 0;
}

void frame_timing_push(double frame_seconds)
{
    if (g_frame_time_count == FRAME_TIME_HISTORY_COUNT) {
        g_frame_time_sum -= g_frame_times[g_frame_time_index];
    } else {
        g_frame_time_count++;
    }

    g_frame_times[g_frame_time_index] = frame_seconds;
    g_frame_time_sum += frame_seconds;
    g_frame_time_index = (g_frame_time_index + 1) % FRAME_TIME_HISTORY_COUNT;
}

double frame_timing_average(void)
{
    if (g_frame_time_count <= 0) {
        return 0.0;
    }

    return g_frame_time_sum / (double)g_frame_time_count;
}

int frame_timing_sample_count(void)
{
    return g_frame_time_count;
}
