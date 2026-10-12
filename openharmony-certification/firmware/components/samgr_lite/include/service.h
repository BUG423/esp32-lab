/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_SERVICE_H
#define OH_SERVICE_H

#include "iunknown.h"
#include "message.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SHARED_TASK = 0,
    SPECIFIED_TASK = 1,
    NO_TASK = 2
} TaskPriority;

typedef struct {
    int16_t  level;
    int16_t  priority;
    uint16_t stackSize;
    uint16_t queueSize;
    uint8_t  taskFlags;
} TaskConfig;

typedef struct Service Service;
struct Service {
    const char *(*GetName)(Service *service);
    bool (*Initialize)(Service *service, Identity identity);
    bool (*MessageHandle)(Service *service, Request *request);
    TaskConfig (*GetTaskConfig)(Service *service);
};

#ifdef __cplusplus
}
#endif

#endif /* OH_SERVICE_H */
