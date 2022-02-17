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
 * @param temperatures pointer to array of temperatures to be stored. This should be the 
 * same size as TEMP_SENSOR_COUNT
 */
void getTemperatures(float *temperatures)
{
    uint8_t readAttempts = 0;

    ds18b20_requestTemperatures();

    for(uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    {
        temperatures[i] = DEVICE_DISCONNECTED;

        while((readAttempts++ < MAX_READ_ATTEMPTS) && tempIsDisconnected(temperatures[i]))
        {
            temperatures[i] = ds18b20_getTempC(&TempSensors[i]);

            if(tempIsDisconnected(temperatures[i]))
            {
                LOGW("Error Reading Temperature %d Attempt %d", i, readAttempts);
            }
            else
            {
                break;
            }
        }

        readAttempts = 0;
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

    for(uint8_t i = 0; i < TEMP_SENSOR_COUNT; i++)
    {
        LOGI("Temperature Address %d: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x", i, TempSensors[i][0],TempSensors[i][1],TempSensors[i][2],TempSensors[i][3],TempSensors[i][4],TempSensors[i][5],TempSensors[i][6],TempSensors[i][7]);
    }
    
    return ESP_OK;
}



