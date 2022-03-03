/**
 * @file pumpControl.c
 * @brief 
 * Contains the main control logic for controlling the Pool Pump
 */

// Project Includes
#include "pumpControl.h"
#include "temperature.h"
#include "ProjectConfig.h"
#include "projectLog.h"

// FreeRTOS Includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

// ESP SDK Includes
#include "esp_log.h"
#include "driver/gpio.h"

// Standard Library Includes
#include <stdbool.h>

#define USE_DEBUG_TIMES 1

#define TIMER_QUEUE_LEN 10

#define PUMP_TASK_PERIOD_SEC 10         // 10 Second Period
#define PUMP_TASK_TICKS pdMS_TO_TICKS(PUMP_TASK_PERIOD_SEC*1000)

// Private function prototypes
void pumpTimerCallback( TimerHandle_t xTimer );
void pumpOn();
void pumpOff();
void updatePumpStateTime();
PumpState_t temperatureControlLogic();
PumpState_t scheduleControlLogic();
void pumpStateControlLogic();

// Statically allocated timer
StaticTimer_t taskProcessTimerBuffer;
TimerHandle_t taskProcessTimer;

// Statically allocated queue
StaticQueue_t timerQueueBuffer;
QueueHandle_t timerQueue;
uint8_t timerQueueStorage[TIMER_QUEUE_LEN];

// Temperature Control variables
float MinAmbientTemperature = 38.0f;
float AmbientTempHysteresis = 2.0f;
float MinWaterTemperature = 35.0f;
float WaterTempHysteresis = 4.0f;

// Pump Control Variables
PumpState_t PumpState = PUMP_STATE_OFF;
uint32_t PumpStateTimeSecs = 0;
uint32_t MinPumpRunTimeSec = 15 * 60;   // 15 Minute Default
uint32_t MinPumpOffTimeSec = 2 * 60;    // 2 Minute Default

float GetMinAmbientTemperature()
{
    return MinAmbientTemperature;
}

float GetAmbientTempHysteresis()
{
    return AmbientTempHysteresis;
}

float GetMinWaterTemperature()
{
    return MinWaterTemperature;
}

float GetWaterTempHysteresis()
{
    return WaterTempHysteresis;
}


void SetMinAmbientTemperature(float temp)
{
    // Don't allow setting of temperature at freezing or below
    if(temp > 1.0)
    {
        MinAmbientTemperature = temp;
    }
}

void SetAmbientTempHysteresis(float hysteresis)
{
    if(hysteresis > 1.0f)
    {
        AmbientTempHysteresis = hysteresis;
    }
}

void SetMinWaterTemperature(float temp)
{
    // Don't allow setting of temperature at freezing or below
    if(temp > 1.0)
    {
        MinWaterTemperature = temp;
    }
}

void SetWaterTempHysteresis(float hysteresis)
{
    if(hysteresis > 1.0f)
    {
        WaterTempHysteresis = hysteresis;
    }
}


void pumpTimerCallback( TimerHandle_t timerHandle )
{
    uint8_t queueData = 4;
    xTimerStop(taskProcessTimer, 0);
    xQueueSend(timerQueue, &queueData, 0);
}

/**
 * @brief Init Function for Pump Control
 * 
 */
void PumpControlInit()
{
#if(USE_DEBUG_TIMES)
    MinPumpRunTimeSec = 10;
    MinPumpOffTimeSec = 5;
#endif

    // Init GPIO for pump control
    gpio_pad_select_gpio(PUMP_GPIO);
    gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);

    // Init Task Timer and Queue
    taskProcessTimer = xTimerCreateStatic("PumpTimer", pdMS_TO_TICKS(100000), false, (void*)0, pumpTimerCallback, &taskProcessTimerBuffer);
    timerQueue = xQueueCreateStatic(TIMER_QUEUE_LEN, sizeof(uint8_t), timerQueueStorage, &timerQueueBuffer);
}

/**
 * @brief Turns pump on and updates tracking variables and logs
 */
void pumpOn()
{
    // Enforce Minimum On Time
    if(PumpStateTimeSecs >= MinPumpRunTimeSec)
    {
        // Reset State Time only if Pump was previously not running
        if(!PumpRunning())
        {
            PumpStateTimeSecs = 0;
            gpio_set_level(PUMP_GPIO, 1);
            PumpState = true;
        }
    }
}

/**
 * @brief Turns pump off and updates tracking variables and logs
 */
void pumpOff()
{
    // Enforce Minimum Off Time
    if(PumpStateTimeSecs >= MinPumpOffTimeSec)
    {
        // Reset State Time only if Pump was previously running
        if(PumpRunning())
        {
            PumpStateTimeSecs = 0;
            gpio_set_level(PUMP_GPIO, 0);
            PumpState = false;
        }
    }
}

/**
 * @brief Get State of Pump
 * 
 * @return true if pump is running
 * @return false if pump is not running
 */
bool PumpRunning()
{
    return PumpState;
}

/**
 * @brief Logic for pump control based on temperature
 * 
 * @return PumpState_t state to put pump in based on temperature
 */
PumpState_t temperatureControlLogic()
{
    float waterTemperature;
    float ambientTemperature;

    /**
     * These Pump State variables are static so that the last decisions are remembered across
     * subsequent calls to this function. In the event that a temperature sensor is disconnected
     * it's last state will be reused. For instance if the pump was turned on due to low temperature
     * and the sensor is disconnected, it will continue to assume that the temperature is low
     * until the sensor is reconnected.
     */
    static PumpState_t ambientTempPumpState = PUMP_STATE_OFF;
    static PumpState_t waterTempPumpState = PUMP_STATE_OFF;

    waterTemperature = getLastTemperatureRead(WATER_TEMP_SENSOR);
    ambientTemperature = getLastTemperatureRead(AMBIENT_TEMP_SENSOR);

    // Don't run ambient temperature logic if sensor is disconnected
    if(!tempIsDisconnected(ambientTemperature))
    {
        if(!PumpRunning())
        {
            if(ambientTemperature < MinAmbientTemperature)
            {
                ambientTempPumpState = PUMP_STATE_ON;
            }
        }
        else    // Pump is Running
        {
            if(ambientTemperature > (MinAmbientTemperature + AmbientTempHysteresis))
            {
                ambientTempPumpState = PUMP_STATE_OFF;
            }
        }
    }

    // Don't run water temperature logic if sensor is disconnected
    if(!tempIsDisconnected(waterTemperature))
    {
        if(!PumpRunning())
        {
            if(waterTemperature < MinWaterTemperature)
            {
                waterTempPumpState = PUMP_STATE_ON;
            }
        }
        else    // Pump is Running
        {
            if(waterTemperature > (MinWaterTemperature + WaterTempHysteresis))
            {
                waterTempPumpState = PUMP_STATE_OFF;
            }
        }
    }

    return waterTempPumpState || ambientTempPumpState;
}

/**
 * @brief Logic for pump control based on time
 * 
 * @return PumpState_t state to put pump in based on schedule
 */
PumpState_t scheduleControlLogic()
{
    return false;   // Not implemented yet
}

/**
 * @brief Increments the Pump State Timer
 */
void updatePumpStateTime()
{
    PumpStateTimeSecs += PUMP_TASK_PERIOD_SEC;
}

/**
 * @brief Top Level function for running pump control logic
 */
void pumpStateControlLogic()
{
    PumpState_t temperaturePumpState;
    PumpState_t schedulePumpState;
    PumpState_t commandedPumpState;

    temperaturePumpState = temperatureControlLogic();
    schedulePumpState = scheduleControlLogic();

    commandedPumpState = temperaturePumpState | schedulePumpState;

    if(commandedPumpState == PUMP_STATE_ON)
    {
        pumpOn();
    }
    else    // commandedPumpState == PUMP_STATE_OFF
    {
        pumpOff();
    }
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
    const TickType_t FunctionPeriod = PUMP_TASK_TICKS;

    uint8_t queueValue;

    // Initialise the LastWakeTime variable with the current time.
    LastWakeTime = xTaskGetTickCount();

    xTimerChangePeriod(taskProcessTimer, pdMS_TO_TICKS(2000), 0);

    while(true)
    {
        // Wait for the next execution cycle.
        vTaskDelayUntil( &LastWakeTime, FunctionPeriod );

        // Timer experation check
        if(xQueueReceive(timerQueue, &queueValue, 0))
        {
            ESP_LOGI("test", "Timer Expired, queueValue: %d", queueValue);
            xTimerChangePeriod(taskProcessTimer, pdMS_TO_TICKS(2000), 0);
        }

        updatePumpStateTime();
        pumpStateControlLogic();
    }
}