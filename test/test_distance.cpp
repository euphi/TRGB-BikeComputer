// test/test_distance.cpp
#include "Stats/Distance.h"  // Stelle sicher, dass diese Datei deine Distance-Klasse enthält
#include <iostream>
#include <stdio.h>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>  // Wird nur bei ESP32 eingebunden
#include <Wire.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <Update.h>
#include <SPI.h>
#else
#include <cstdio>  // Standard C-Header für printf/putchar auf Desktop-Systemen
#endif
#include "unity_config.h"

#include <unity.h>

Distance distance;

void setUp() {
    std::cout << "Setting up test..." << std::endl;
    //distance = Distance(); // Setze das Objekt zurück
}

void tearDown() {
    std::cout << "Tearing down test..." << std::endl;
}

void test_distance_initial_state() {
    std::cout << "Running test_distance_initial_state..." << std::endl;
    TEST_ASSERT_EQUAL_FLOAT(0.0, distance.getDistance(Statistics::SUM_ESP_TOTAL));
}

void test_distance_update_revs_no_reset() {
    std::cout << "Running test_distance_update_revs_no_reset..." << std::endl;
    distance.updateRevs(100, 1.5); // Simulierter Wert
    std::cout << "Total Distance after update: " << distance.getDistance(Statistics::SUM_ESP_TOTAL) << std::endl;
    TEST_ASSERT_GREATER_THAN(0.0, distance.getDistance(Statistics::SUM_ESP_TOTAL));
}

void test_distance_update_revs_reset_case() {
    std::cout << "Running test_distance_update_revs_reset_case..." << std::endl;
    distance.updateRevs(100, 1.5);
    float dist_before = distance.getDistance(Statistics::SUM_ESP_TOTAL);
    std::cout << "Distance before reset scenario: " << dist_before << std::endl;
    distance.updateRevs(10, 1.5);  // Simuliert Reset auf niedrigen Wert
    std::cout << "Distance after reset scenario: " << distance.getDistance(Statistics::SUM_ESP_TOTAL) << std::endl;
    TEST_ASSERT_GREATER_THAN(dist_before, distance.getDistance(Statistics::SUM_ESP_TOTAL));
}

void setup()
{
    delay(2000);
//int main() {
    UNITY_BEGIN();
    std::cout << "Starting Unity Tests..." << std::endl;
    RUN_TEST(test_distance_initial_state);
    RUN_TEST(test_distance_update_revs_no_reset);
    RUN_TEST(test_distance_update_revs_reset_case);
    std::cout << "All tests completed." << std::endl;
    /*return*/ UNITY_END();
}


void loop()
{
    delay(1000);
}
