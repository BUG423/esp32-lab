/*
 * Copyright (c) 2026 OpenHarmony Certification Project
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include "gpio_if.h"
#include "hdf_device_desc.h"
#include "hdf_manager.h"
#include "hdf_log.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include <string.h>

#define MAX_GPIO_PINS 40

typedef struct {
    GpioIrqFunc func;
    void *arg;
    bool isrRegistered;
} GpioIrqEntry;

static GpioIrqEntry s_irqEntries[MAX_GPIO_PINS];
static bool s_isrServiceInstalled = false;

static void IRAM_ATTR EspGpioIsrHandler(void *arg)
{
    uint16_t pin = (uint16_t)(uintptr_t)arg;
    if (pin < MAX_GPIO_PINS && s_irqEntries[pin].func) {
        s_irqEntries[pin].func(pin, s_irqEntries[pin].arg);
    }
}

int32_t GpioSetDir(uint16_t gpio, uint16_t dir)
{
    if (gpio >= GPIO_NUM_MAX) {
        return HDF_ERR_INVALID_PARAM;
    }
    gpio_mode_t mode = (dir == GPIO_DIR_OUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
    esp_err_t err = gpio_set_direction((gpio_num_t)gpio, mode);
    return (err == ESP_OK) ? HDF_SUCCESS : HDF_FAILURE;
}

int32_t GpioGetDir(uint16_t gpio, uint16_t *dir)
{
    if (gpio >= GPIO_NUM_MAX || dir == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    /* ESP32 doesn't have direct direction read, default output check */
    *dir = GPIO_DIR_OUT;
    return HDF_SUCCESS;
}

int32_t GpioWrite(uint16_t gpio, uint16_t val)
{
    if (gpio >= GPIO_NUM_MAX) {
        return HDF_ERR_INVALID_PARAM;
    }
    esp_err_t err = gpio_set_level((gpio_num_t)gpio, (uint32_t)val);
    return (err == ESP_OK) ? HDF_SUCCESS : HDF_FAILURE;
}

int32_t GpioRead(uint16_t gpio, uint16_t *val)
{
    if (gpio >= GPIO_NUM_MAX || val == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }
    *val = (uint16_t)gpio_get_level((gpio_num_t)gpio);
    return HDF_SUCCESS;
}

int32_t GpioSetIrq(uint16_t gpio, uint16_t mode, GpioIrqFunc func, void *arg)
{
    if (gpio >= MAX_GPIO_PINS || func == NULL) {
        return HDF_ERR_INVALID_PARAM;
    }

    if (!s_isrServiceInstalled) {
        gpio_install_isr_service(0);
        s_isrServiceInstalled = true;
    }

    gpio_int_type_t intType = GPIO_INTR_DISABLE;
    switch (mode) {
        case GPIO_INT_RISING:  intType = GPIO_INTR_POSEDGE; break;
        case GPIO_INT_FALLING: intType = GPIO_INTR_NEGEDGE; break;
        case GPIO_INT_BOTH:    intType = GPIO_INTR_ANYEDGE; break;
        default: break;
    }

    gpio_set_intr_type((gpio_num_t)gpio, intType);
    s_irqEntries[gpio].func = func;
    s_irqEntries[gpio].arg = arg;
    s_irqEntries[gpio].isrRegistered = true;

    gpio_isr_handler_add((gpio_num_t)gpio, EspGpioIsrHandler, (void *)(uintptr_t)gpio);
    return HDF_SUCCESS;
}

int32_t GpioUnsetIrq(uint16_t gpio)
{
    if (gpio >= MAX_GPIO_PINS) {
        return HDF_ERR_INVALID_PARAM;
    }
    gpio_intr_disable((gpio_num_t)gpio);
    if (s_irqEntries[gpio].isrRegistered) {
        gpio_isr_handler_remove((gpio_num_t)gpio);
        s_irqEntries[gpio].isrRegistered = false;
        s_irqEntries[gpio].func = NULL;
    }
    return HDF_SUCCESS;
}

int32_t GpioEnableIrq(uint16_t gpio)
{
    if (gpio >= GPIO_NUM_MAX) {
        return HDF_ERR_INVALID_PARAM;
    }
    gpio_intr_enable((gpio_num_t)gpio);
    return HDF_SUCCESS;
}

int32_t GpioDisableIrq(uint16_t gpio)
{
    if (gpio >= GPIO_NUM_MAX) {
        return HDF_ERR_INVALID_PARAM;
    }
    gpio_intr_disable((gpio_num_t)gpio);
    return HDF_SUCCESS;
}

static int32_t HdfGpioDriverInit(struct HdfDeviceObject *deviceObject)
{
    (void)deviceObject;
    memset(s_irqEntries, 0, sizeof(s_irqEntries));
    HDF_LOGI("HDF GPIO platform driver initialized");
    return HDF_SUCCESS;
}

static struct HdfDriverEntry s_gpioDriverEntry = {
    .driverName = "HDF_PLATFORM_GPIO",
    .Init = HdfGpioDriverInit,
    .Bind = NULL,
    .Release = NULL,
};

static void __attribute__((constructor)) RegisterGpioDriver(void)
{
    HdfRegisterDriver(&s_gpioDriverEntry);
}
