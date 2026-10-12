/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "kvstore_env.h"
#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "hilog/log.h"

#define HILOG_MODULE "KVStore"
#define KV_NAMESPACE "oh_kv"

static nvs_handle_t s_nvsHandle = 0;
static SemaphoreHandle_t s_kvMutex = NULL;
static bool s_isInitialized = false;

int KVStoreInit(void)
{
    if (s_isInitialized) {
        return EC_SUCCESS;
    }

    if (s_kvMutex == NULL) {
        s_kvMutex = xSemaphoreCreateMutex();
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        HILOG_ERROR(LOG_CORE, "NVS flash init failed: 0x%x", err);
        return EC_FAILURE;
    }

    err = nvs_open(KV_NAMESPACE, NVS_READWRITE, &s_nvsHandle);
    if (err != ESP_OK) {
        HILOG_ERROR(LOG_CORE, "NVS open failed: 0x%x", err);
        return EC_FAILURE;
    }

    s_isInitialized = true;
    HILOG_INFO(LOG_CORE, "KVStore Lite engine initialized successfully");
    return EC_SUCCESS;
}

int UtilsGetValue(const char *key, char *value, unsigned int len)
{
    if (key == NULL || value == NULL || len == 0 || strlen(key) > MAX_KEY_LEN) {
        return EC_INVALID_PARAM;
    }

    if (!s_isInitialized) {
        if (KVStoreInit() != EC_SUCCESS) return EC_FAILURE;
    }

    xSemaphoreTake(s_kvMutex, portMAX_DELAY);
    size_t reqLen = len;
    esp_err_t err = nvs_get_str(s_nvsHandle, key, value, &reqLen);
    xSemaphoreGive(s_kvMutex);

    if (err == ESP_OK) {
        return EC_SUCCESS;
    }
    return EC_FAILURE;
}

int UtilsSetValue(const char *key, const char *value)
{
    if (key == NULL || value == NULL || strlen(key) > MAX_KEY_LEN || strlen(value) > MAX_VALUE_LEN) {
        return EC_INVALID_PARAM;
    }

    if (!s_isInitialized) {
        if (KVStoreInit() != EC_SUCCESS) return EC_FAILURE;
    }

    xSemaphoreTake(s_kvMutex, portMAX_DELAY);
    esp_err_t err = nvs_set_str(s_nvsHandle, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvsHandle);
    }
    xSemaphoreGive(s_kvMutex);

    return (err == ESP_OK) ? EC_SUCCESS : EC_FAILURE;
}

int UtilsDeleteValue(const char *key)
{
    if (key == NULL || strlen(key) > MAX_KEY_LEN) {
        return EC_INVALID_PARAM;
    }

    if (!s_isInitialized) {
        if (KVStoreInit() != EC_SUCCESS) return EC_FAILURE;
    }

    xSemaphoreTake(s_kvMutex, portMAX_DELAY);
    esp_err_t err = nvs_erase_key(s_nvsHandle, key);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvsHandle);
    }
    xSemaphoreGive(s_kvMutex);

    return (err == ESP_OK) ? EC_SUCCESS : EC_FAILURE;
}

int UtilsClearValues(void)
{
    if (!s_isInitialized) {
        if (KVStoreInit() != EC_SUCCESS) return EC_FAILURE;
    }

    xSemaphoreTake(s_kvMutex, portMAX_DELAY);
    esp_err_t err = nvs_erase_all(s_nvsHandle);
    if (err == ESP_OK) {
        err = nvs_commit(s_nvsHandle);
    }
    xSemaphoreGive(s_kvMutex);

    return (err == ESP_OK) ? EC_SUCCESS : EC_FAILURE;
}
