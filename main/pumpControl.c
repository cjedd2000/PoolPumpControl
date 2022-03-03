/**
 * @file pumpControl.c
 * @brief 
 * Contains the main control logic for controlling the Pool Pump
 */

#include "pumpControl.h"

// FreeRTOS Includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_log.h"

#define TIMER_QUEUE_LEN 10

// Private function prototypes
void pumpTimerCallback( TimerHandle_t xTimer );

// Statically allocated timer
StaticTimer_t taskProcessTimerBuffer;
TimerHandle_t taskProcessTimer;

// Statically allocated queue
StaticQueue_t timerQueueBuffer;
QueueHandle_t timerQueue;
uint8_t timerQueueStorage[TIMER_QUEUE_LEN];

void pumpTimerCallback( TimerHandle_t timerHandle )
{
    uint8_t queueData = 4;
    xTimerStop(taskProcessTimer, 0);
    xQueueSend(timerQueue, &queueData, 0);
}

void pumpControlInit()
{
    taskProcessTimer = xTimerCreateStatic("PumpTimer", pdMS_TO_TICKS(100000), false, (void*)0, pumpTimerCallback, &taskProcessTimerBuffer);
    timerQueue = xQueueCreateStatic(TIMER_QUEUE_LEN, sizeof(uint8_t), timerQueueStorage, &timerQueueBuffer);
}

/**
 * @brief 
 * Main Pump Control Logic Task
 * 
 * @param parameters Unused
 */
void PumpControlTask(void * parameters)
{
    TickType_t LastWakeTime;
    const TickType_t FunctionPeriod = pdMS_TO_TICKS(1000);      // 5 Second Delay

    uint8_t queueValue;

    // Initialise the LastWakeTime variable with the current time.
    LastWakeTime = xTaskGetTickCount();

    xTimerChangePeriod(taskProcessTimer, pdMS_TO_TICKS(2000), 0);

    while(true)
    {
        // Wait for the next cycle.
        vTaskDelayUntil( &LastWakeTime, FunctionPeriod );

        if(xQueueReceive(timerQueue, &queueValue, 0))
        {
            ESP_LOGI("test", "Timer Expired, queueValue: %d", queueValue);
            xTimerChangePeriod(taskProcessTimer, pdMS_TO_TICKS(2000), 0);
        }
    }
}