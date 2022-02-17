/**
 * @file main.c
 * @brief 
 * Main application file. Configures system and starts all tasks.
 */

// ESP IDF Includes
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "esp_spiffs.h"
#include "esp_wifi.h"

// Project Inclues
#include "ProjectConfig.h"
#include "WifiConfig.h"
#include "webServer.h"
#include "temperature.h"
#include "ds18b20.h"
#include "projectLog.h"

// FreeRTOS Includes
#include "freertos/task.h"

/**
 * @brief 
 * Union used to reinterpret floats and uint32_t types for data transmission over sockets.
 */
typedef union
{
    uint32_t i;
    float f;
} data32_t;

/**
 * @brief 
 * Attempts to re connect to WiFi when it disconnects.
 */
static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    LOGI("Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

/**
 * @brief 
 * For future IPv6 support
 */
static void on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    esp_netif_create_ip6_linklocal(esp_netif);
}

/**
 * @brief 
 * Logs IP Address
 */
static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    LOGI("Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
}

/**
 * @brief 
 * Logs IPv6 Address
 */
static void on_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;

    LOGI("Got IPv6 event: Interface \"%s\" address: " IPV6STR "", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip));
}

/**
 * @brief 
 * Starts WiFi Connection
 * 
 * @return esp_netif_t* 
 */
static esp_netif_t *wifi_start(void)
{
    char *desc;
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Prefix the interface description with the module TAG
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    asprintf(&desc, "%s: %s", "wifi_start", esp_netif_config.if_desc);
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t *netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    free(desc);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
#ifdef CONFIG_EXAMPLE_CONNECT_IPV6
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_wifi_connect, netif));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_GOT_IP6, &on_got_ipv6, NULL));
#endif

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };
    LOGI("Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
    return netif;
}

/**
 * @brief 
 * Configures MDNS
 */
static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set("PoolPumpMdnsInst");

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

/**
 * @brief 
 * Configures file system to be used for http server
 * 
 * @return esp_err_t 
 */
esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 7,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            LOGE("Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            LOGE("Failed to find SPIFFS partition");
        } else {
            LOGE("Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        LOGE("Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        LOGI("Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

/**
 * @brief 
 * Temporary function for testing various features.
 * 
 * @param parameters Unused
 */
void Periodic5SecFuncs(void * parameters)
{
    TickType_t LastWakeTime;
    const TickType_t FunctionPeriod = 5 * xPortGetTickRateHz();      // 5 Second Delay

    bool temp = false;

    data32_t ambientTemperature, waterTemperature;

    static char buffer[100];
    static char buffer2[500];

    // Initialise the LastWakeTime variable with the current time.
    LastWakeTime = xTaskGetTickCount();

    while(true)
    {
        // Wait for the next cycle.
        vTaskDelayUntil( &LastWakeTime, FunctionPeriod );

        // Actions
        getTemperatures(&ambientTemperature.f, &waterTemperature.f);

        snprintf(buffer, 100, "Temperatures:\nAmbient: %0.1fC\n  Water: %0.1fC", ambientTemperature.f, waterTemperature.f);
        LOGI("%s", buffer);

        sendData(WS_DATA_WATER_TEMP, waterTemperature.i);
        sendData(WS_DATA_AMB_TEMP, ambientTemperature.i);

        if(temp)
            sendData(WS_DATA_PUMP_STATE, 1);
        else
            sendData(WS_DATA_PUMP_STATE, 0);

        temp = !temp;

        vTaskList(buffer2);
        //LOGI("\nTaskList:\n%s\n\n", buffer2);    
    }
}

/**
 * @brief 
 * Entry Point for ESP IDF. Default task created. Automatically deleted by SDK if returns.
 */
void app_main(void)
{
    configureTempSensors();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(MDNS_HOST_NAME);

    wifi_start();
    ESP_ERROR_CHECK(init_fs());

    ESP_ERROR_CHECK(start_web_server(WEB_MOUNT_POINT));

    // Start testing task
    xTaskCreate(&Periodic5SecFuncs, "5SecFuncs", ESP_TASK_MAIN_STACK, NULL, 10, NULL);
}
