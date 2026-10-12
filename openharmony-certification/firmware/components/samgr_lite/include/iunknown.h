/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_IUNKNOWN_H
#define OH_IUNKNOWN_H

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_VERSION 0x0001

typedef struct IUnknown IUnknown;
struct IUnknown {
    int (*QueryInterface)(IUnknown *iUnknown, int version, void **target);
    int (*AddRef)(IUnknown *iUnknown);
    int (*Release)(IUnknown *iUnknown);
};

#define INHERIT_IUNKNOWN \
    int (*QueryInterface)(IUnknown *iUnknown, int version, void **target); \
    int (*AddRef)(IUnknown *iUnknown); \
    int (*Release)(IUnknown *iUnknown)

#ifdef __cplusplus
}
#endif

#endif /* OH_IUNKNOWN_H */
