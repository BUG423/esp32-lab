/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "uart_if.h"
#include "hdf_device_desc.h"
#include "hdf_manager.h"
#include "hdf_log.h"
#include "driver/uart.h"
#include <string.h>

#define MAX_UART_PORTS 3

typedef struct {
    uint32_t port;
    bool opened;
    uint32_t baudRate;
} EspUartDevice;

static EspUartDevice s_uartDevices[MAX_UART_PORTS] = {
    { .port = 0, .opened = false, .baudRate = 115200 },
    { .port = 1, .opened = false, .baudRate = 115200 },
    { .port = 2, .opened = false, .baudRate = 115200 }
};

DevHandle UartOpen(uint32_t port)
{
    if (port >= MAX_UART_PORTS) {
        return NULL;
    }

    EspUartDevice *dev = &s_uartDevices[port];
    if (dev->opened) {
        return (DevHandle)dev;
    }

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    if (port != UART_NUM_0) {
        /* UART0 is console, install for secondary ports */
        uart_driver_install((uart_port_t)port, 1024, 1024, 0, NULL, 0);
        uart_param_config((uart_port_t)port, &uart_config);
    }

    dev->opened = true;
    HDF_LOGI("UART%lu opened at %lubps", (unsigned long)port, (unsigned long)dev->baudRate);
    return (DevHandle)dev;
}

void UartClose(DevHandle handle)
{
    if (handle == NULL) return;
    EspUartDevice *dev = (EspUartDevice *)handle;
    if (dev->port != UART_NUM_0 && dev->opened) {
        uart_driver_delete((uart_port_t)dev->port);
    }
    dev->opened = false;
    HDF_LOGI("UART%lu closed", (unsigned long)dev->port);
}

int32_t UartRead(DevHandle handle, uint8_t *data, uint32_t size)
{
    if (handle == NULL || data == NULL || size == 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    EspUartDevice *dev = (EspUartDevice *)handle;
    int len = uart_read_bytes((uart_port_t)dev->port, data, size, pdMS_TO_TICKS(100));
    return (len >= 0) ? (int32_t)len : HDF_FAILURE;
}

int32_t UartWrite(DevHandle handle, const uint8_t *data, uint32_t size)
{
    if (handle == NULL || data == NULL || size == 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    EspUartDevice *dev = (EspUartDevice *)handle;
    int len = uart_write_bytes((uart_port_t)dev->port, (const char *)data, size);
    return (len >= 0) ? (int32_t)len : HDF_FAILURE;
}

int32_t UartSetBaud(DevHandle handle, uint32_t baudRate)
{
    if (handle == NULL || baudRate == 0) {
        return HDF_ERR_INVALID_PARAM;
    }
    EspUartDevice *dev = (EspUartDevice *)handle;
    esp_err_t err = uart_set_baudrate((uart_port_t)dev->port, baudRate);
    if (err == ESP_OK) {
        dev->baudRate = baudRate;
        return HDF_SUCCESS;
    }
    return HDF_FAILURE;
}

int32_t UartGetBaud(DevHandle handle, uint32_t *baudRate)
{
    if (handle == NULL || baudRate == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    EspUartDevice *dev = (EspUartDevice *)handle;
    *baudRate = dev->baudRate;
    return HDF_SUCCESS;
}

static int32_t HdfUartDriverInit(struct HdfDeviceObject *deviceObject)
{
    (void)deviceObject;
    HDF_LOGI("HDF UART platform driver initialized");
    return HDF_SUCCESS;
}

static struct HdfDriverEntry s_uartDriverEntry = {
    .driverName = "HDF_PLATFORM_UART",
    .Init = HdfUartDriverInit,
    .Bind = NULL,
    .Release = NULL,
};

static void __attribute__((constructor)) RegisterUartDriver(void)
{
    HdfRegisterDriver(&s_uartDriverEntry);
}
