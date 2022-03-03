/**
 * @file sysTime.c
 * 
 * @brief 
 * Manages the system time and RTC
 * 
 */

#include <time.h>
#include <stdbool.h>
#include "esp_sntp.h"

#include "projectLog.h"

// Private Function prototypes
void time_sync_notification_cb(struct timeval *tv);

/**
 * @brief Checks if the time has been set since startup
 * 
 * @return true if time is set false otherwise
 */
bool isTimeSet()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900))
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * @brief Callback function when a time sync occurs for logging purposes
 * 
 * @param tv required parameter for callback
 */
void time_sync_notification_cb(struct timeval *tv)
{
   // ESP_LOGI("sysTime.c", "NTP Time Sync Occurred.");
}

/**
 * @brief Generate a string for the current time
 * 
 * @param buffer char buffer to place string into
 * @param bufLen length of buffer passed
 */
void getTimeStr(char * buffer, size_t bufLen)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(buffer, bufLen, "%c", &timeinfo);
}

/**
 * @brief 
 * Initializes the RTC and SNTP functions
 * 
 * @param timezone timezone to set the system to
 */
void timeInit(char* timezone)
{
    LOGI("Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    // Set timezone to Eastern Standard Time and print local time
    setenv("TZ", timezone, 1);
    tzset();
}
