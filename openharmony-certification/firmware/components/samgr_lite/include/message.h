/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_MESSAGE_H
#define OH_MESSAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t queueId;
    int16_t featureId;
} Identity;

typedef struct {
    int16_t  msgId;
    int16_t  msgValue;
    void     *data;
    uint32_t len;
} Request;

typedef struct {
    void     *data;
    uint32_t len;
} Response;

typedef void (*Handler)(const Request *request, const Response *response);

#ifdef __cplusplus
}
#endif

#endif /* OH_MESSAGE_H */
