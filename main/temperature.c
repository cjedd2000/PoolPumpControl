#include "ProjectConfig.h"
#include "ds18b20.h"
#include "esp_err.h"
#include "projectLog.h"
#include "temperature.h"

// Only Temporary
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

DeviceAddress tempSensors[TEMP_SENSOR_COUNT];
unsigned int sensorsFound = 0;

void getTempAddresses(DeviceAddress *tempSensorAddresses) {
	
	reset_search();

    while (search(tempSensorAddresses[sensorsFound],true)) {
		sensorsFound++;
		if (sensorsFound == 2) break;
	}

    if(sensorsFound != 2)
    {
        LOGW("Expected 2 Temperature Sensors. Only Found %d", sensorsFound);
    }
}

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

void getTemperatures(float *ambTemp, float *waterTemp)
{
    uint8_t readAttempts = 0;
    *ambTemp = DEVICE_DISCONNECTED;
    *waterTemp = DEVICE_DISCONNECTED;

    ds18b20_requestTemperatures();

    while((readAttempts++ < MAX_READ_ATTEMPTS) && tempIsDisconnected(*ambTemp))
    {
        *ambTemp = ds18b20_getTempC((DeviceAddress *)tempSensors[AMBIENT_TEMP_SENSOR]);

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
        *waterTemp = ds18b20_getTempC((DeviceAddress *)tempSensors[WATER_TEMP_SENSOR]);

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

esp_err_t configureTempSensors()
{
    ds18b20_init(TEMP_SENSOR_ONE_WIRE_GPIO);
	getTempAddresses(tempSensors);
	ds18b20_setResolution(tempSensors,2,10);

    LOGI("Temperature Address 0: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[0][0],tempSensors[0][1],tempSensors[0][2],tempSensors[0][3],tempSensors[0][4],tempSensors[0][5],tempSensors[0][6],tempSensors[0][7]);
    LOGI("Temperature Address 1: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[1][0],tempSensors[1][1],tempSensors[1][2],tempSensors[1][3],tempSensors[1][4],tempSensors[1][5],tempSensors[1][6],tempSensors[1][7]);

    return ESP_OK;
}



