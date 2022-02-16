#define MAX_READ_ATTEMPTS 5

esp_err_t configureTempSensors();
void getTemperatures();
bool tempIsDisconnected(float temperature);

typedef enum 
{
    AMBIENT_TEMP_SENSOR = 0,
    WATER_TEMP_SENSOR,
    TEMP_SENSOR_COUNT
} TempSensorId;