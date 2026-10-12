/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_SAMGR_LITE_H
#define OH_SAMGR_LITE_H

#include "service.h"
#include "feature.h"
#include "iunknown.h"
#include "message.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EC_SUCCESS    0
#define EC_INVALID   -1
#define EC_NOPERM    -2
#define EC_NOMEMORY  -3

typedef struct SamgrLite SamgrLite;
struct SamgrLite {
    bool (*RegisterService)(Service *service);
    Service *(*UnregisterService)(const char *name);
    bool (*RegisterFeature)(const char *serviceName, Feature *feature);
    Feature *(*UnregisterFeature)(const char *serviceName, const char *featureName);
    bool (*RegisterFeatureApi)(const char *serviceName, const char *featureName, IUnknown *api);
    IUnknown *(*GetFeatureApi)(const char *serviceName, const char *featureName);
    IUnknown *(*GetDefaultFeatureApi)(const char *serviceName);
    int32_t (*SendRequest)(const Identity *identity, const Request *request, Handler handler);
    int32_t (*SendResponse)(const Request *request, const Response *response);
};

SamgrLite *SAMGR_GetInstance(void);
void SAMGR_Bootstrap(void);

#define SYS_SERVICE_INIT(func) \
    static void __attribute__((constructor)) __Init_##func(void) { func(); }

#ifdef __cplusplus
}
#endif

#endif /* OH_SAMGR_LITE_H */
