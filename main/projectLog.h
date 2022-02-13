#include "esp_log.h"

#define LOGI(Message, ...) ESP_LOGI(__FILE__, Message, ##__VA_ARGS__)
#define LOGW(Message, ...) ESP_LOGW(__FILE__, Message, ##__VA_ARGS__)
#define LOGE(Message, ...) ESP_LOGE(__FILE__, Message, ##__VA_ARGS__)