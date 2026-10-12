/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_KVSTORE_ENV_H
#define OH_KVSTORE_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

#define EC_SUCCESS        0
#define EC_FAILURE       -1
#define EC_INVALID_PARAM -2

#define MAX_KEY_LEN     32
#define MAX_VALUE_LEN   1024

int UtilsGetValue(const char *key, char *value, unsigned int len);
int UtilsSetValue(const char *key, const char *value);
int UtilsDeleteValue(const char *key);
int UtilsClearValues(void);
int KVStoreInit(void);

#ifdef __cplusplus
}
#endif

#endif /* OH_KVSTORE_ENV_H */
