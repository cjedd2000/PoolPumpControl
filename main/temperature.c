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

	// search for 2 addresses on the oneWire protocol
	//if(search(tempSensorAddresses[sensorsFound],true)) sensorsFound++;
    //if(search(tempSensorAddresses[sensorsFound],true)) sensorsFound++;

    while (search(tempSensorAddresses[sensorsFound],true)) {
		sensorsFound++;
		if (sensorsFound == 2) break;
	}

    if(sensorsFound != 2)
    {
        LOGW("Expected 2 Temperature Sensors. Only Found %d", sensorsFound);
    }
}

void getTemperatures(float *ambTemp, float *waterTemp)
{
    ds18b20_requestTemperatures();
    *ambTemp = ds18b20_getTempC((DeviceAddress *)tempSensors[AMBIENT_TEMP_SENSOR]);
    *waterTemp = ds18b20_getTempC((DeviceAddress *)tempSensors[WATER_TEMP_SENSOR]);
    LOGI("Ambient Temperature: %0.1fC", *ambTemp);
    LOGI("Water Temperature: %0.1fC", *waterTemp);
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



