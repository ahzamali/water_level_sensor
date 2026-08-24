#include <unity.h>
#include <Arduino.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers that mirror the real global config char arrays
// ─────────────────────────────────────────────────────────────────────────────
static char tank_bottom_distance[6] = "98";
static char max_water_level[6]      = "75";
static char speed_of_sound[6]       = "342";

// ─────────────────────────────────────────────────────────────────────────────
// Group 1 — String conversion parity
// Verify that atol/atof produce identical results to String(x).toInt/toFloat
// for every config value used in main.cpp loop calculations.
// ─────────────────────────────────────────────────────────────────────────────

void test_atol_parity_tank_bottom_distance(void) {
    long expected = String(tank_bottom_distance).toInt();
    long actual   = atol(tank_bottom_distance);
    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

void test_atol_parity_max_water_level(void) {
    long expected = String(max_water_level).toInt();
    long actual   = atol(max_water_level);
    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

void test_atof_parity_speed_of_sound(void) {
    float expected = String(speed_of_sound).toFloat();
    float actual   = atof(speed_of_sound);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, expected, actual);
}

void test_atol_parity_water_level_calculation(void) {
    // Simulates: long water_level = atol(tank_bottom_distance) - distance_to_water;
    long distance_to_water = 45L;
    long expected = String(tank_bottom_distance).toInt() - distance_to_water;
    long actual   = atol(tank_bottom_distance) - distance_to_water;
    TEST_ASSERT_EQUAL_INT32(expected, actual);
}

void test_atof_parity_percentage_calculation(void) {
    // Simulates: float pct = ((float)water_level / max_level_float) * 100.0f
    long water_level = 53L;
    float old_result = ((float)water_level / String(max_water_level).toFloat()) * 100.0f;
    float new_result = ((float)water_level / atof(max_water_level)) * 100.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, old_result, new_result);
}

void test_atol_edge_case_leading_whitespace(void) {
    // Ensure atol handles values that might be stored with leading whitespace
    char buf[6] = " 98";
    TEST_ASSERT_EQUAL_INT32(98, atol(buf));
}

// ─────────────────────────────────────────────────────────────────────────────
// Group 2 — JSON output correctness with StaticJsonBuffer
// Verify that StaticJsonBuffer<512> correctly serializes all /api/readings
// fields and that the output fits within the 512-byte char response buffer.
// ─────────────────────────────────────────────────────────────────────────────

void test_static_json_buffer_has_required_keys(void) {
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["distance_to_water"]        = 45L;
    root["water_level"]              = 53L;
    root["normalized_level"]         = 6;
    root["percentage"]               = 70.7f;
    root["speed_of_sound"]           = atol(speed_of_sound);
    root["tank_bottom_distance"]     = atol(tank_bottom_distance);
    root["max_water_level"]          = atol(max_water_level);
    root["temperature"]              = 28.5f;
    root["humidity"]                 = 65.0f;
    root["pressure"]                 = 1013.25f;
    root["mqtt_connected"]           = true;
    root["mqtt_debug_enabled"]       = false;
    root["stay_awake_remaining_sec"] = 0;
    root["uptime_sec"]               = 3600;
    root["wifi_rssi"]                = -65;
    root["wifi_ssid"]                = "MyNetwork";
    root["ip_address"]               = "192.168.1.100";

    TEST_ASSERT_TRUE(root.containsKey("distance_to_water"));
    TEST_ASSERT_TRUE(root.containsKey("water_level"));
    TEST_ASSERT_TRUE(root.containsKey("percentage"));
    TEST_ASSERT_TRUE(root.containsKey("temperature"));
    TEST_ASSERT_TRUE(root.containsKey("humidity"));
    TEST_ASSERT_TRUE(root.containsKey("mqtt_connected"));
    TEST_ASSERT_TRUE(root.containsKey("uptime_sec"));
    TEST_ASSERT_TRUE(root.containsKey("ip_address"));
}

void test_static_json_buffer_values_are_correct(void) {
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["distance_to_water"]    = 45L;
    root["water_level"]          = 53L;
    root["percentage"]           = 70.7f;
    root["tank_bottom_distance"] = atol(tank_bottom_distance);
    root["max_water_level"]      = atol(max_water_level);

    TEST_ASSERT_EQUAL_INT32(45, root["distance_to_water"].as<long>());
    TEST_ASSERT_EQUAL_INT32(53, root["water_level"].as<long>());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 70.7f, root["percentage"].as<float>());
    TEST_ASSERT_EQUAL_INT32(98, root["tank_bottom_distance"].as<long>());
    TEST_ASSERT_EQUAL_INT32(75, root["max_water_level"].as<long>());
}

void test_static_json_response_fits_in_512_chars(void) {
    // Worst-case full response — all fields at maximum value widths
    StaticJsonBuffer<512> jsonBuffer;
    JsonObject& root = jsonBuffer.createObject();

    root["distance_to_water"]        = 99L;
    root["water_level"]              = -1L;
    root["normalized_level"]         = 9;
    root["percentage"]               = 100.0f;
    root["speed_of_sound"]           = 342;
    root["tank_bottom_distance"]     = 98;
    root["max_water_level"]          = 75;
    root["temperature"]              = 45.55f;
    root["humidity"]                 = 99.99f;
    root["pressure"]                 = 1055.55f;
    root["mqtt_connected"]           = false;
    root["mqtt_debug_enabled"]       = true;
    root["stay_awake_remaining_sec"] = 599;
    root["uptime_sec"]               = 999999;
    root["wifi_rssi"]                = -95;
    root["wifi_ssid"]                = "MyLongNetworkNameHere";
    root["ip_address"]               = "192.168.100.200";

    char response[512];
    size_t written = root.printTo(response, sizeof(response));

    // printTo returns 0 on buffer overflow in ArduinoJson v5
    TEST_ASSERT_GREATER_THAN(0, (int)written);
    // Must fit within 512 bytes including null terminator
    TEST_ASSERT_LESS_THAN(512, (int)written);
}

void test_mqtt_client_id_snprintf_format(void) {
    // Verify new char[]-based client_id matches old String-based output exactly
    uint32_t chip_id = 0xABCDEF;

    // Old approach:
    String old_id = "roof_water_sensor_" + String(chip_id, HEX);

    // New approach:
    char new_id[32];
    snprintf(new_id, sizeof(new_id), "roof_water_sensor_%x", chip_id);

    TEST_ASSERT_EQUAL_STRING(old_id.c_str(), new_id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Runner
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
    delay(2000);
    UNITY_BEGIN();

    // Group 1: String conversion parity
    RUN_TEST(test_atol_parity_tank_bottom_distance);
    RUN_TEST(test_atol_parity_max_water_level);
    RUN_TEST(test_atof_parity_speed_of_sound);
    RUN_TEST(test_atol_parity_water_level_calculation);
    RUN_TEST(test_atof_parity_percentage_calculation);
    RUN_TEST(test_atol_edge_case_leading_whitespace);

    // Group 2: JSON output correctness
    RUN_TEST(test_static_json_buffer_has_required_keys);
    RUN_TEST(test_static_json_buffer_values_are_correct);
    RUN_TEST(test_static_json_response_fits_in_512_chars);
    RUN_TEST(test_mqtt_client_id_snprintf_format);

    UNITY_END();
}

void loop() {}
