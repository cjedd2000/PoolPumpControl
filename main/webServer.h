#pragma once

#include "esp_err.h"

typedef enum
{
    WS_DATA_NONE = 0,
    WS_DATA_PUMP_STATE,
    WS_DATA_WATER_TEMP,
    WS_DATA_AMB_TEMP
} wsDataType_t;

esp_err_t start_web_server(const char *base_path);
void sendToRemoteDebugger(const char *format, ...);
void sendData(wsDataType_t dataType, uint32_t data);