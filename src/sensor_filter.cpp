#include "sensor_filter.h"

long calculateMedian(const long samples[], int count, long fallbackValue) {
  if (count <= 0 || samples == NULL) {
    return fallbackValue;
  }

  // Copy samples to temporary array for sorting
  long temp[32];
  int n = count > 32 ? 32 : count;
  for (int i = 0; i < n; i++) {
    temp[i] = samples[i];
  }

  // Insertion Sort
  for (int i = 1; i < n; i++) {
    long key = temp[i];
    int j = i - 1;
    while (j >= 0 && temp[j] > key) {
      temp[j + 1] = temp[j];
      j--;
    }
    temp[j + 1] = key;
  }

  return temp[n / 2];
}

long applyEma(long current_smoothed, long new_reading, float alpha) {
  if (new_reading <= 0) {
    return current_smoothed;
  }
  if (current_smoothed <= 0) {
    return new_reading;
  }
  if (alpha < 0.0f) alpha = 0.0f;
  if (alpha > 1.0f) alpha = 1.0f;

  return (long)(alpha * (float)new_reading + (1.0f - alpha) * (float)current_smoothed);
}

bool isSampleValid(float dist, long tank_max) {
  if (dist <= 1.0f) {
    return false;
  }
  if (tank_max > 0 && dist > (tank_max + 50)) {
    return false;
  }
  return true;
}

int calculateNormalizedLevel(long water_level, float max_water_level, int led_count) {
  if (max_water_level <= 0 || led_count <= 0) {
    return 0;
  }

  int normalized = (int)(led_count * ((float)water_level / max_water_level));
  if (normalized < 0) {
    normalized = 0;
  }
  if (normalized > led_count) {
    normalized = led_count;
  }

  return normalized;
}
