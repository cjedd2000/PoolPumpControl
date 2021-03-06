#pragma once
/**
 * @file temperature.h
 * 
 * @brief 
 * Wrapper around driver for temperature sensor to handle all temperature functions and error checking
 * 
 */

// ESP IDF includes
#include "esp_err.h"

// Standard Library Includes
#include <stdbool.h>

/* Max attempts to make at reading temperature sensor when an error occurs before reporting Device Disconnected */
#define MAX_READ_ATTEMPTS 5

/* Number of bits resolution for temperature sensors */
#define SENSOR_RESOLUTION 10

/* Temperature Sensor ID Enumeration */
typedef enum 
{
    AMBIENT_TEMP_SENSOR = 0,
    WATER_TEMP_SENSOR,
    TEMP_SENSOR_COUNT
} TempSensorId;

/* Public Function Prototypes */
esp_err_t configureTempSensors();
void getTemperatures(float *temperatures);
bool tempIsDisconnected(float temperature);
float getLastTemperatureRead(TempSensorId sensorId);

