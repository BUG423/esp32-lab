/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_HDF_LOG_H
#define OH_HDF_LOG_H

#include "hilog/log.h"

#define HDF_LOG_TAG "HDF_LITE"

#define HDF_LOGD(fmt, ...) HiLogPrint(LOG_CORE, LOG_DEBUG, 0xD002500, HDF_LOG_TAG, fmt, ##__VA_ARGS__)
#define HDF_LOGI(fmt, ...) HiLogPrint(LOG_CORE, LOG_INFO,  0xD002500, HDF_LOG_TAG, fmt, ##__VA_ARGS__)
#define HDF_LOGW(fmt, ...) HiLogPrint(LOG_CORE, LOG_WARN,  0xD002500, HDF_LOG_TAG, fmt, ##__VA_ARGS__)
#define HDF_LOGE(fmt, ...) HiLogPrint(LOG_CORE, LOG_ERROR, 0xD002500, HDF_LOG_TAG, fmt, ##__VA_ARGS__)

#endif /* OH_HDF_LOG_H */
