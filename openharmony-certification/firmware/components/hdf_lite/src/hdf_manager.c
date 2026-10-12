/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "hdf_manager.h"
#include "hdf_log.h"
#include <string.h>

#define MAX_HDF_DRIVERS 16

static const struct HdfDriverEntry *s_driverRegistry[MAX_HDF_DRIVERS];
static uint32_t s_driverCount = 0;

int32_t HdfRegisterDriver(const struct HdfDriverEntry *entry)
{
    if (entry == NULL || entry->driverName == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    if (s_driverCount >= MAX_HDF_DRIVERS) {
        return HDF_FAILURE;
    }

    s_driverRegistry[s_driverCount++] = entry;
    HDF_LOGI("Registered HDF driver: %s", entry->driverName);
    return HDF_SUCCESS;
}

int32_t HdfDeviceManagerInit(void)
{
    HDF_LOGI("HDF DeviceManager initializing platform hardware drivers...");
    for (uint32_t i = 0; i < s_driverCount; i++) {
        const struct HdfDriverEntry *entry = s_driverRegistry[i];
        if (entry != NULL) {
            struct HdfDeviceObject devObj = {
                .deviceName = entry->driverName,
                .priv = NULL
            };
            if (entry->Bind) {
                entry->Bind(&devObj);
            }
            if (entry->Init) {
                entry->Init(&devObj);
            }
            HDF_LOGI("Driver [%s] bound and initialized", entry->driverName);
        }
    }
    HDF_LOGI("HDF DeviceManager ready (Drivers loaded: %lu)", (unsigned long)s_driverCount);
    return HDF_SUCCESS;
}
