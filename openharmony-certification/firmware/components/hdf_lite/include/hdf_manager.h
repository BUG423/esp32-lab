/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_HDF_MANAGER_H
#define OH_HDF_MANAGER_H

#include "hdf_base.h"
#include "hdf_device_desc.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t HdfDeviceManagerInit(void);
int32_t HdfRegisterDriver(const struct HdfDriverEntry *entry);

#ifdef __cplusplus
}
#endif

#endif /* OH_HDF_MANAGER_H */
