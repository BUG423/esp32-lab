/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef OH_UART_IF_H
#define OH_UART_IF_H

#include "hdf_base.h"

#ifdef __cplusplus
extern "C" {
#endif

DevHandle UartOpen(uint32_t port);
void UartClose(DevHandle handle);
int32_t UartRead(DevHandle handle, uint8_t *data, uint32_t size);
int32_t UartWrite(DevHandle handle, const uint8_t *data, uint32_t size);
int32_t UartSetBaud(DevHandle handle, uint32_t baudRate);
int32_t UartGetBaud(DevHandle handle, uint32_t *baudRate);

#ifdef __cplusplus
}
#endif

#endif /* OH_UART_IF_H */
