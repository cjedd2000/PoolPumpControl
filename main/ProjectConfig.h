#pragma once
/**
 * @file ProjectConfig.h
 * 
 * @brief 
 * Top Level Configurations for Project
 * 
 */


// Web Server
#define MDNS_HOST_NAME "PoolPumpCtrl"
#define WEB_MOUNT_POINT "/www"

// Logging
#define ENABLE_REMOTE_DEBUGGER 1

// Time
#define TIMEZONE "EST5EDT,M3.2.0/2,M11.1.0"

// IO
#define TEMP_SENSOR_ONE_WIRE_GPIO 26
#define LED_GPIO 2