/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "hilog/log.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define HILOG_BUFFER_SIZE 256

static LogLevel s_minLevel = LOG_DEBUG;
static uint32_t s_logCount = 0;
static SemaphoreHandle_t s_logMutex = NULL;

static const char *GetLevelStr(LogLevel level)
{
    switch (level) {
        case LOG_DEBUG: return "\033[0;36mDEBUG\033[0m";
        case LOG_INFO:  return "\033[0;32mINFO\033[0m ";
        case LOG_WARN:  return "\033[0;33mWARN\033[0m ";
        case LOG_ERROR: return "\033[0;31mERROR\033[0m";
        case LOG_FATAL: return "\033[1;31mFATAL\033[0m";
        default:        return "UNKWN";
    }
}

void HiLogInit(void)
{
    if (s_logMutex == NULL) {
        s_logMutex = xSemaphoreCreateMutex();
    }
    s_minLevel = LOG_DEBUG;
    s_logCount = 0;
}

void HiLogSetMinLevel(LogLevel level)
{
    s_minLevel = level;
}

LogLevel HiLogGetMinLevel(void)
{
    return s_minLevel;
}

int HiLogGetRecordCount(void)
{
    return (int)s_logCount;
}

int HiLogPrint(LogType type, LogLevel level, unsigned int domain, const char *tag, const char *fmt, ...)
{
    (void)type;
    if (level < s_minLevel) {
        return 0;
    }

    char messageBuf[HILOG_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(messageBuf, sizeof(messageBuf), fmt, args);
    va_end(args);

    if (s_logMutex != NULL) {
        xSemaphoreTake(s_logMutex, portMAX_DELAY);
    }

    int64_t uptimeMs = esp_timer_get_time() / 1000;
    uint32_t sec = (uint32_t)(uptimeMs / 1000);
    uint32_t ms  = (uint32_t)(uptimeMs % 1000);

    /* OpenHarmony standard logging header format */
    printf("[%5lu.%03lu][OH_%s][0x%07x][%s]: %s\n",
           (unsigned long)sec,
           (unsigned long)ms,
           GetLevelStr(level),
           domain,
           tag ? tag : "DEFAULT",
           messageBuf);

    s_logCount++;

    if (s_logMutex != NULL) {
        xSemaphoreGive(s_logMutex);
    }

    return written;
}
