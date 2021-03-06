#pragma once
/**
 * @file projectLog.h
 * 
 * @brief 
 * Wrapper around ESP_LOG* functions that automatically set a tag from the function name.
 * Also sends out logs on the debugger websocket when running.
 * 
 */

// SDK Includes
#include "esp_log.h"
#include <string.h>

// Project Includes
#include "webServer.h"
#include "ProjectConfig.h"

#if(ENABLE_REMOTE_DEBUGGER != 1)
    #define LOGI(Message, ...) ESP_LOGI(__FUNCTION__, Message, ##__VA_ARGS__)
    #define LOGW(Message, ...) ESP_LOGW(__FUNCTION__, Message, ##__VA_ARGS__)
    #define LOGE(Message, ...) ESP_LOGE(__FUNCTION__, Message, ##__VA_ARGS__)
#else
    #define LOGI(Message, ...) ESP_LOGI(__FUNCTION__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
    #define LOGW(Message, ...) ESP_LOGW(__FUNCTION__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
    #define LOGE(Message, ...) ESP_LOGE(__FUNCTION__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
#endif