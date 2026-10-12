/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_FEATURE_H
#define OH_FEATURE_H

#include "iunknown.h"
#include "message.h"
#include "service.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Feature Feature;
struct Feature {
    const char *(*GetName)(Feature *feature);
    void (*OnInitialize)(Feature *feature, Service *parent, Identity identity);
    void (*OnStop)(Feature *feature, Identity identity);
    bool (*OnMessage)(Feature *feature, Request *request);
};

#ifdef __cplusplus
}
#endif

#endif /* OH_FEATURE_H */
