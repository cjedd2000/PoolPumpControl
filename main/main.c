#include <stdio.h>
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "esp_vfs_semihost.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "esp_wifi.h"

#include "ProjectConfig.h"
#include "WifiConfig.h"
#include "webServer.h"

#include "ds18b20.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static void on_wifi_disconnect(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    ESP_LOGI("on_wifi_disconnect", "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
        return;
    }
    ESP_ERROR_CHECK(err);
}

static void on_wifi_connect(void *esp_netif, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    esp_netif_create_ip6_linklocal(esp_netif);
}

// Edited this function
static void on_got_ip(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI("on_got_ip", "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
}

static void on_got_ipv6(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;

    ESP_LOGI("on_got_ipv6", "Got IPv6 event: Interface \"%s\" address: " IPV6STR "", esp_netif_get_desc(event->esp_netif),
             IPV62STR(event->ip6_info.ip));
}

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
    ESP_LOGI("wifi_start", "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_connect();
    return netif;
}

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

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("init_fs", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("init_fs", "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE("init_fs", "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE("init_fs", "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI("init_fs", "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}


void sendFunction(void * parameters)
{
    char buffer[50];

    uint count = 0;

    while(true)
    {
        vTaskDelay(3000/portTICK_PERIOD_MS);
        snprintf(buffer, 49, "Count = %d", count++);
        sendNewDataToSockets(buffer, strlen(buffer));
    }
}

DeviceAddress tempSensors[2];
unsigned int sensorsFound = 0;

void getTempAddresses(DeviceAddress *tempSensorAddresses) {
	
	reset_search();

	// search for 2 addresses on the oneWire protocol
	while (search(tempSensorAddresses[sensorsFound],true)) {
		sensorsFound++;
		if (sensorsFound == 1) break;
	}
	// if 2 addresses aren't found then flash the LED rapidly
	while (sensorsFound != 1) {
		sensorsFound = 0;
		vTaskDelay(100 / portTICK_PERIOD_MS);
		vTaskDelay(100 / portTICK_PERIOD_MS);
		// search in the loop for the temp sensors as they may hook them up
		reset_search();
		while (search(tempSensorAddresses[sensorsFound],true)) {
			sensorsFound++;
			if (sensorsFound == 1) break;
		}
	}
	return;
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(MDNS_HOST_NAME);

    //ESP_ERROR_CHECK(example_connect());
    wifi_start();
    ESP_ERROR_CHECK(init_fs());

    ESP_ERROR_CHECK(start_web_server(WEB_MOUNT_POINT));

    //xTaskCreate(sendFunction, "Send Func", ESP_TASK_MAIN_STACK, NULL, ESP_TASK_MAIN_PRIO, NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI("temp", "______________________________Here___________________");
    ESP_LOGI("temp", "Init BUs");
    ds18b20_init(TEMP_SENSOR_ONE_WIRE_GPIO);
    ESP_LOGI("temp", "Get Addresses");
	getTempAddresses(tempSensors);
    ESP_LOGI("temp", "Set Resolution");
	ds18b20_setResolution(tempSensors,2,10);

    ESP_LOGI("temp", "Address 0: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[0][0],tempSensors[0][1],tempSensors[0][2],tempSensors[0][3],tempSensors[0][4],tempSensors[0][5],tempSensors[0][6],tempSensors[0][7]);
    ESP_LOGI("temp", "Address 1: 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \n", tempSensors[1][0],tempSensors[1][1],tempSensors[1][2],tempSensors[1][3],tempSensors[1][4],tempSensors[1][5],tempSensors[1][6],tempSensors[1][7]);

    while (1) {
		ds18b20_requestTemperatures();
		float temp3 = ds18b20_getTempC((DeviceAddress *)tempSensors[0]);
		float temp4 = ds18b20_getTempC((DeviceAddress *)tempSensors[1]);
		ESP_LOGI("temp", "Temperatures: %0.1fC %0.1fC\n", temp3,temp4);

		float cTemp = ds18b20_get_temp();
		ESP_LOGI("temp", "Temperature: %0.1fC\n", cTemp);
		vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}
