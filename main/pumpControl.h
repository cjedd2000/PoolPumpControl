#pragma once
/**
 * @file pumpControl.h
 * @brief 
 * Contains the main control logic for controlling the Pool Pump
 */
#include <stdbool.h>

typedef enum {
    PUMP_STATE_ON = true,
    PUMP_STATE_OFF = false
} PumpState_t;

// Main Public Functions
void PumpControlInit();
void PumpControlTask(void * parameters);
bool PumpRunning();

// Configuration Getters and Setters
float GetMinAmbientTemperature();
float GetAmbientTempHysteresis();
float GetMinWaterTemperature();
float GetWaterTempHysteresis();

void SetMinAmbientTemperature(float temp);
void SetAmbientTempHysteresis(float hysteresis);
void SetMinWaterTemperature(float temp);
void SetWaterTempHysteresis(float hysteresis);
