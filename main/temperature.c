/**
 * @file temperature.c
 * 
 * @brief 
 * Wrapper around driver for temperature sensor to handle all temperature functions and error checking
 * 
 */

// Project Includes
#include "ProjectConfig.h"
#include "projectLog.h"
#include "temperature.h"


// DS18B20 Driver
#include "ds18b20.h"

/* List of Connected Device Addresses */
DeviceAddress TempSensors[TEMP_SENSOR_COUNT];

/* Current number of sensors known on bus */
unsigned int SensorsFound = 0;

/**
 * @brief Searches bus for available temperature sensors
 * 
 * @param tempSensorAddresses Array to store device addresses in
 */
void getTempAddresses(DeviceAddress *tempSensorAddresses) {
	
	reset_search();

    // Search through all addresses or until all sensors found
    while (search(tempSensorAddresses[SensorsFound],true)) {
		SensorsFound++;
		if (SensorsFound == TEMP_SENSOR_COUNT) break;
	}

    // Got to End of search and didn't find expected number of sensors
    if(SensorsFound != TEMP_SENSOR_COUNT)
    {
        LOGW("Expected %d Temperature Sensors. Only Found %d", TEMP_SENSOR_COUNT, SensorsFound);
    }
}

/**
 * @brief 
 * Checks to see if reported temperature means sensor is disconnected
 * 
 * @param temperature temperature to check
 * 
 * @return true if device is considered disconnected, or false otherwise.
 */
bool tempIsDisconnected(float temperature)
{
    if(temperature < (DEVICE_DISCONNECTED + 1.0f))
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief 
 * Read temperatures from connected temperature sensors
 * 
 * @param ambTemp 
 * @param waterTemp 
 */
void getTemperatures(float *ambTemp, float *waterTemp)
{
    uint8_t readAttempts = 0;
    *ambTemp = DEVICE_DISCONNECTED;
    *waterTemp = DEVICE_DISCONNECTED;

    ds18b20_requestTemperatures();

    while((readAttempts++ < MAX_READ_ATTEMPTS) && tempIsDisconnected(*ambTemp))
    {
        *ambTemp = ds18b20_getTempC((DeviceAddress *)TempSensors[AMBIENT_TEMP_SENSOR]);

        if(*ambTemp < (DEVICE_DISCONNECTED + 1.0f))
        {
            LOGW("Error Reading Ambient Temperature Attempt %d", readAttempts);
        }
        else
        {
            break;
        }
    }

    readAttempts = 0;
    
    while((readAttempts++ < MAX_READ_ATTEMPTS) && tempIsDisconnected(*waterTemp))
    {
        *waterTemp = ds18b20_getTempC((DeviceAddress *)TempSensors[WATER_TEMP_SENSOR]);

        if(*waterTemp < (DEVICE_DISCONNECTED + 1.0f))
        {
            LOGW("Error Reading Water Temperature Attempt %d", readAttempts);
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief 
 * Configure One Wire Bus, search for and configure temperature sensors.
 * 
 * @return esp_err_t 
 */
esp_err_t configureTempSensors()
{
    ds18b20_init(TEMP_SENSOR_ONE_WIRE_GPIO);
	getTempAddresses(TempSensors);
	ds18b20_setResolution(TempSensors, TEMP_SENSOR_COUNT, SENSOR_RESOLUTION);

    LOGI("Temperature Address 0: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", TempSensors[0][0],TempSensors[0][1],TempSensors[0][2],TempSensors[0][3],TempSensors[0][4],TempSensors[0][5],TempSensors[0][6],TempSensors[0][7]);
    LOGI("Temperature Address 1: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", TempSensors[1][0],TempSensors[1][1],TempSensors[1][2],TempSensors[1][3],TempSensors[1][4],TempSensors[1][5],TempSensors[1][6],TempSensors[1][7]);

    return ESP_OK;
}



