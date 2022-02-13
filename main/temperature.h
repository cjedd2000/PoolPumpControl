esp_err_t configureTempSensors();
void getTemperatures();

typedef enum 
{
    AMBIENT_TEMP_SENSOR = 0,
    WATER_TEMP_SENSOR,
    TEMP_SENSOR_COUNT
} TempSensorId;