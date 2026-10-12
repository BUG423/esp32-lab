/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "los_kernel.h"
#include <stdio.h>

UINT32 LOS_KernelInit(VOID)
{
    /* Under ESP-IDF, FreeRTOS kernel is initialized during startup */
    return LOS_OK;
}

UINT32 LOS_KernelStart(VOID)
{
    /* FreeRTOS scheduler is already running when app_main is called */
    return LOS_OK;
}
