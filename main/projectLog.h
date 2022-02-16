//#pragma once

#include "esp_log.h"
#include "ProjectConfig.h"
#include "webServer.h"
#include <string.h>

#if(ENABLE_REMOTE_DEBUGGER != 1)
    #define LOGI(Message, ...) ESP_LOGI(__FILE__, Message, ##__VA_ARGS__)
    #define LOGW(Message, ...) ESP_LOGW(__FILE__, Message, ##__VA_ARGS__)
    #define LOGE(Message, ...) ESP_LOGE(__FILE__, Message, ##__VA_ARGS__)
#else
    #define LOGI(Message, ...) ESP_LOGI(__FILE__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
    #define LOGW(Message, ...) ESP_LOGW(__FILE__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
    #define LOGE(Message, ...) ESP_LOGE(__FILE__, Message, ##__VA_ARGS__); if(strcmp(__FUNCTION__, "sendToRemoteDebugger") != 0) sendToRemoteDebugger(Message, ##__VA_ARGS__);
#endif