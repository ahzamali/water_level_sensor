#include <unity.h>
#include "sensor_filter.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_median_odd_elements_noise_rejection(void) {
  // Array containing extreme noise: 0, 9999, 20
  long samples[] = {150, 20, 152, 149, 9999, 151, 0, 150, 148};
  long median = calculateMedian(samples, 9, 0);
  // Sorted array: 0, 20, 148, 149, 150, 150, 151, 152, 9999
  // Median index (9/2 = 4) -> 150
  TEST_ASSERT_EQUAL_INT32(150, median);
}

void test_median_even_elements(void) {
  long samples[] = {100, 200, 150, 120};
  // Sorted: 100, 120, 150, 200
  // Index 4/2 = 2 -> 150
  long median = calculateMedian(samples, 4, 0);
  TEST_ASSERT_EQUAL_INT32(150, median);
}

void test_median_empty_fallback(void) {
  long samples[] = {};
  long median = calculateMedian(samples, 0, 100);
  TEST_ASSERT_EQUAL_INT32(100, median);
}

void test_ema_smoothing_initialization(void) {
  // If current_smoothed is 0, EMA should initialize directly to the new reading
  long smoothed = applyEma(0, 150, 0.25f);
  TEST_ASSERT_EQUAL_INT32(150, smoothed);
}

void test_ema_smoothing_computation(void) {
  // Previous distance: 100, new reading: 140, alpha: 0.25
  // Expected: 0.25 * 140 + 0.75 * 100 = 35 + 75 = 110
  long smoothed = applyEma(100, 140, 0.25f);
  TEST_ASSERT_EQUAL_INT32(110, smoothed);
}

void test_sample_validity_outliers(void) {
  // Invalid readings
  TEST_ASSERT_FALSE(isSampleValid(0.0f, 150));
  TEST_ASSERT_FALSE(isSampleValid(1.0f, 150));
  TEST_ASSERT_FALSE(isSampleValid(210.0f, 150)); // > 150 + 50 = 200

  // Valid readings
  TEST_ASSERT_TRUE(isSampleValid(50.0f, 150));
  TEST_ASSERT_TRUE(isSampleValid(150.0f, 150));
}

void test_normalized_level_clamping(void) {
  // Standard inside bounds: water_level = 60, max = 120, led_count = 9
  // (60 / 120) * 9 = 4.5 -> 4
  TEST_ASSERT_EQUAL_INT(4, calculateNormalizedLevel(60, 120.0f, 9));

  // Full tank: water_level = 120
  TEST_ASSERT_EQUAL_INT(9, calculateNormalizedLevel(120, 120.0f, 9));

  // Overflow/Upper clamp: water_level = 150 > max 120
  TEST_ASSERT_EQUAL_INT(9, calculateNormalizedLevel(150, 120.0f, 9));

  // Underflow/Lower clamp: water_level = -10
  TEST_ASSERT_EQUAL_INT(0, calculateNormalizedLevel(-10, 120.0f, 9));
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_median_odd_elements_noise_rejection);
  RUN_TEST(test_median_even_elements);
  RUN_TEST(test_median_empty_fallback);
  RUN_TEST(test_ema_smoothing_initialization);
  RUN_TEST(test_ema_smoothing_computation);
  RUN_TEST(test_sample_validity_outliers);
  RUN_TEST(test_normalized_level_clamping);
  UNITY_END();
}

void loop() {
}
