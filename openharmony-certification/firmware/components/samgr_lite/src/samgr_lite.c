/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "samgr_lite.h"
#include "hilog/log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#define MAX_SERVICES 16
#define MAX_FEATURES 8

#define HILOG_MODULE "SAMGR"

typedef struct {
    Feature *feature;
    IUnknown *api;
    bool inUse;
} FeatureEntry;

typedef struct {
    Service *service;
    FeatureEntry features[MAX_FEATURES];
    QueueHandle_t msgQueue;
    bool inUse;
} ServiceEntry;

static ServiceEntry s_services[MAX_SERVICES];
static SemaphoreHandle_t s_samgrMutex = NULL;
static bool s_samgrInitialized = false;

static int FindServiceIndex(const char *name)
{
    if (name == NULL) return -1;
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (s_services[i].inUse && s_services[i].service != NULL) {
            const char *sName = s_services[i].service->GetName(s_services[i].service);
            if (sName && strcmp(sName, name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

static bool SamgrRegisterService(Service *service)
{
    if (service == NULL || service->GetName == NULL) {
        return false;
    }

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    const char *name = service->GetName(service);
    if (FindServiceIndex(name) >= 0) {
        xSemaphoreGive(s_samgrMutex);
        return false; /* Already exists */
    }

    int freeIdx = -1;
    for (int i = 0; i < MAX_SERVICES; i++) {
        if (!s_services[i].inUse) {
            freeIdx = i;
            break;
        }
    }

    if (freeIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return false;
    }

    s_services[freeIdx].service = service;
    s_services[freeIdx].inUse = true;
    memset(s_services[freeIdx].features, 0, sizeof(s_services[freeIdx].features));
    s_services[freeIdx].msgQueue = xQueueCreate(16, sizeof(Request));

    xSemaphoreGive(s_samgrMutex);

    Identity id = { .queueId = (int16_t)freeIdx, .featureId = -1 };
    if (service->Initialize) {
        service->Initialize(service, id);
    }

    HILOG_INFO(LOG_CORE, "Registered service: %s (id: %d)", name, freeIdx);
    return true;
}

static Service *SamgrUnregisterService(const char *name)
{
    if (name == NULL) return NULL;

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    int idx = FindServiceIndex(name);
    if (idx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return NULL;
    }

    Service *svc = s_services[idx].service;
    if (s_services[idx].msgQueue) {
        vQueueDelete(s_services[idx].msgQueue);
        s_services[idx].msgQueue = NULL;
    }
    s_services[idx].inUse = false;
    s_services[idx].service = NULL;
    xSemaphoreGive(s_samgrMutex);

    HILOG_INFO(LOG_CORE, "Unregistered service: %s", name);
    return svc;
}

static bool SamgrRegisterFeature(const char *serviceName, Feature *feature)
{
    if (serviceName == NULL || feature == NULL || feature->GetName == NULL) {
        return false;
    }

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    int sIdx = FindServiceIndex(serviceName);
    if (sIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return false;
    }

    int fIdx = -1;
    for (int i = 0; i < MAX_FEATURES; i++) {
        if (!s_services[sIdx].features[i].inUse) {
            fIdx = i;
            break;
        }
    }

    if (fIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return false;
    }

    s_services[sIdx].features[fIdx].feature = feature;
    s_services[sIdx].features[fIdx].api = NULL;
    s_services[sIdx].features[fIdx].inUse = true;
    xSemaphoreGive(s_samgrMutex);

    Identity id = { .queueId = (int16_t)sIdx, .featureId = (int16_t)fIdx };
    if (feature->OnInitialize) {
        feature->OnInitialize(feature, s_services[sIdx].service, id);
    }

    HILOG_INFO(LOG_CORE, "Registered feature: %s into service %s", feature->GetName(feature), serviceName);
    return true;
}

static Feature *SamgrUnregisterFeature(const char *serviceName, const char *featureName)
{
    if (serviceName == NULL || featureName == NULL) return NULL;

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    int sIdx = FindServiceIndex(serviceName);
    if (sIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return NULL;
    }

    for (int i = 0; i < MAX_FEATURES; i++) {
        if (s_services[sIdx].features[i].inUse && s_services[sIdx].features[i].feature) {
            Feature *f = s_services[sIdx].features[i].feature;
            if (f->GetName && strcmp(f->GetName(f), featureName) == 0) {
                s_services[sIdx].features[i].inUse = false;
                s_services[sIdx].features[i].feature = NULL;
                s_services[sIdx].features[i].api = NULL;
                xSemaphoreGive(s_samgrMutex);
                return f;
            }
        }
    }

    xSemaphoreGive(s_samgrMutex);
    return NULL;
}

static bool SamgrRegisterFeatureApi(const char *serviceName, const char *featureName, IUnknown *api)
{
    if (serviceName == NULL || api == NULL) return false;

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    int sIdx = FindServiceIndex(serviceName);
    if (sIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return false;
    }

    if (featureName == NULL) {
        /* Default service API: store in feature index 0 */
        s_services[sIdx].features[0].api = api;
        xSemaphoreGive(s_samgrMutex);
        return true;
    }

    for (int i = 0; i < MAX_FEATURES; i++) {
        if (s_services[sIdx].features[i].inUse && s_services[sIdx].features[i].feature) {
            Feature *f = s_services[sIdx].features[i].feature;
            if (f->GetName && strcmp(f->GetName(f), featureName) == 0) {
                s_services[sIdx].features[i].api = api;
                xSemaphoreGive(s_samgrMutex);
                return true;
            }
        }
    }

    xSemaphoreGive(s_samgrMutex);
    return false;
}

static IUnknown *SamgrGetFeatureApi(const char *serviceName, const char *featureName)
{
    if (serviceName == NULL) return NULL;

    xSemaphoreTake(s_samgrMutex, portMAX_DELAY);
    int sIdx = FindServiceIndex(serviceName);
    if (sIdx < 0) {
        xSemaphoreGive(s_samgrMutex);
        return NULL;
    }

    if (featureName == NULL) {
        IUnknown *api = s_services[sIdx].features[0].api;
        xSemaphoreGive(s_samgrMutex);
        return api;
    }

    for (int i = 0; i < MAX_FEATURES; i++) {
        if (s_services[sIdx].features[i].inUse && s_services[sIdx].features[i].feature) {
            Feature *f = s_services[sIdx].features[i].feature;
            if (f->GetName && strcmp(f->GetName(f), featureName) == 0) {
                IUnknown *api = s_services[sIdx].features[i].api;
                xSemaphoreGive(s_samgrMutex);
                return api;
            }
        }
    }

    xSemaphoreGive(s_samgrMutex);
    return NULL;
}

static IUnknown *SamgrGetDefaultFeatureApi(const char *serviceName)
{
    return SamgrGetFeatureApi(serviceName, NULL);
}

static int32_t SamgrSendRequest(const Identity *identity, const Request *request, Handler handler)
{
    if (identity == NULL || request == NULL) {
        return EC_INVALID;
    }

    int sIdx = identity->queueId;
    if (sIdx < 0 || sIdx >= MAX_SERVICES || !s_services[sIdx].inUse) {
        return EC_INVALID;
    }

    Service *svc = s_services[sIdx].service;
    if (svc && svc->MessageHandle) {
        Request reqCopy = *request;
        bool handled = svc->MessageHandle(svc, &reqCopy);
        if (handler) {
            Response resp = { .data = (void *)(handled ? "SUCCESS" : "FAILED"), .len = 7 };
            handler(&reqCopy, &resp);
        }
        return EC_SUCCESS;
    }

    return EC_INVALID;
}

static int32_t SamgrSendResponse(const Request *request, const Response *response)
{
    (void)request;
    (void)response;
    return EC_SUCCESS;
}

static SamgrLite s_samgrInstance = {
    .RegisterService = SamgrRegisterService,
    .UnregisterService = SamgrUnregisterService,
    .RegisterFeature = SamgrRegisterFeature,
    .UnregisterFeature = SamgrUnregisterFeature,
    .RegisterFeatureApi = SamgrRegisterFeatureApi,
    .GetFeatureApi = SamgrGetFeatureApi,
    .GetDefaultFeatureApi = SamgrGetDefaultFeatureApi,
    .SendRequest = SamgrSendRequest,
    .SendResponse = SamgrSendResponse,
};

SamgrLite *SAMGR_GetInstance(void)
{
    if (!s_samgrInitialized) {
        s_samgrMutex = xSemaphoreCreateMutex();
        memset(s_services, 0, sizeof(s_services));
        s_samgrInitialized = true;
    }
    return &s_samgrInstance;
}

void SAMGR_Bootstrap(void)
{
    SAMGR_GetInstance();
    HILOG_INFO(LOG_CORE, "SAMGR microkernel service framework bootstrap initialized");
}
