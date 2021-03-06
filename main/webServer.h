#pragma once
/**
 * @file webServer.h
 * 
 * @brief 
 * Handles All Webserver functions.
 */

#include "esp_err.h"

#define WS_ALL_CLIENTS (-1)

/**
 * @brief 
 * Union used to reinterpret floats and uint32_t types for data transmission over sockets.
 */
typedef union
{
    uint32_t i;
    float f;
} data32_t;

typedef enum
{
    WS_DATA_NONE = 0,

    // Feedback packets
    WS_DATA_PUMP_STATE,
    WS_DATA_WATER_TEMP,
    WS_DATA_AMB_TEMP,

    // Settings Data
    WS_DATA_SETTING_MIN_AMB,
    WS_DATA_SETTING_AMB_HYST,
    WS_DATA_SETTING_MIN_WATER,
    WS_DATA_SETTING_WATER_HYST,
    WS_DATA_NEW_SETTING_DATA,
} wsDataType_t;

/**
 * @brief Defines the indexes of the settings data packet
 */
typedef enum
{
    SETTINGS_TYPE = 0,
    SETTINGS_MIN_AMB_TEMP,
    SETTINGS_MIN_WATER_TEMP,
    SETTINGS_AMB_HYSTERESIS,
    SETTINGS_WATER_HYSTERESIS,
} SettingsData;

esp_err_t start_web_server(const char *base_path);
void sendToRemoteDebugger(const char *format, ...);
void sendData(wsDataType_t dataType, uint32_t data, int clientFds);