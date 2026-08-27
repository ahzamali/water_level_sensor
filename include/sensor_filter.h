#ifndef SENSOR_FILTER_H
#define SENSOR_FILTER_H

#include <Arduino.h>

// Calculates the median value from an array of samples (ignores <= 0 fallback if needed).
long calculateMedian(const long samples[], int count, long fallbackValue = 0);

// Calculates Exponential Moving Average (EMA) distance.
long applyEma(long current_smoothed, long new_reading, float alpha = 0.25f);

// Validates whether a measured distance reading (cm) is valid.
bool isSampleValid(float dist, long tank_max);

// Calculates normalized level [0..led_count] clamped to valid range.
int calculateNormalizedLevel(long water_level, float max_water_level, int led_count);

#endif // SENSOR_FILTER_H
