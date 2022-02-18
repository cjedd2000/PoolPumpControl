#pragma once
/**
 * @file sysTime.h
 * 
 * @brief 
 * Manages the system time and RTC
 * 
 */

// Public Function Prototypes
void timeInit();
void getTimeStr(char * buffer, size_t bufLen);
bool isTimeSet();
