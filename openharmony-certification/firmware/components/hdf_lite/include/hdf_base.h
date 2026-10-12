/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_HDF_BASE_H
#define OH_HDF_BASE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HDF_SUCCESS            0
#define HDF_FAILURE           -1
#define HDF_ERR_INVALID_PARAM -2
#define HDF_ERR_DEVICE_BUSY   -3
#define HDF_ERR_TIMEOUT       -4
#define HDF_ERR_IO            -5

typedef void *DevHandle;

#ifdef __cplusplus
}
#endif

#endif /* OH_HDF_BASE_H */
