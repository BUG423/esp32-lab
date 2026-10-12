/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_HDF_DEVICE_DESC_H
#define OH_HDF_DEVICE_DESC_H

#include "hdf_base.h"

#ifdef __cplusplus
extern "C" {
#endif

struct HdfDeviceObject {
    const char *deviceName;
    void *priv;
};

struct HdfDriverEntry {
    int32_t (*Init)(struct HdfDeviceObject *deviceObject);
    int32_t (*Bind)(struct HdfDeviceObject *deviceObject);
    void (*Release)(struct HdfDeviceObject *deviceObject);
    const char *driverName;
};

#define HDF_INIT(entry) \
    static const struct HdfDriverEntry *__hdf_driver_##entry __attribute__((used, section(".hdf.driver"))) = &entry

#ifdef __cplusplus
}
#endif

#endif /* OH_HDF_DEVICE_DESC_H */
