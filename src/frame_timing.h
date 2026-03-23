#pragma once

void   frame_timing_reset(void);
void   frame_timing_push(double frame_seconds);
double frame_timing_average(void);
int    frame_timing_sample_count(void);
