/**
 * @file ohos_esp32_config.h
 * @brief OpenHarmony Mini System LiteOS-M & HDF Integration Profile for ESP32
 * @copyright Copyright (c) 2026 OpenHarmony Community & ESP32 Lab
 */

#ifndef OHOS_ESP32_CONFIG_H
#define OHOS_ESP32_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 1. Hardware Architecture & Clock Definitions
 * ============================================================================ */
#define OHOS_CHIP_TARGET_ESP32          1
#define OHOS_ARCH_XTENSA_LX6            1
#define OHOS_CPU_CLOCK_HZ               240000000UL /* 240 MHz */
#define OHOS_INTERNAL_SRAM_BYTES        (520 * 1024)
#define OHOS_TICK_RATE_HZ               1000

/* ============================================================================
 * 2. LiteOS-M Kernel Configuration
 * ============================================================================ */
#define LOSCFG_BASE_CORE_TSK_LIMIT      32
#define LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO 20
#define LOSCFG_BASE_CORE_TSK_MIN_STACK_SIZE 1024
#define LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE 4096
#define LOSCFG_BASE_IPC_MUX_LIMIT       32
#define LOSCFG_BASE_IPC_SEM_LIMIT       32
#define LOSCFG_BASE_IPC_QUEUE_LIMIT     16
#define LOSCFG_BASE_CORE_SWTMR_LIMIT    16
#define LOSCFG_SYS_HEAP_SIZE            (128 * 1024)

/* ============================================================================
 * 3. LittleFS Flash Partition Layout
 * ============================================================================ */
#define OHOS_LFS_PARTITION_LABEL        "data"
#define OHOS_LFS_PARTITION_OFFSET       0x250000
#define OHOS_LFS_PARTITION_SIZE         (1024 * 1024) /* 1 MB */
#define OHOS_LFS_BLOCK_SIZE             4096
#define OHOS_LFS_READ_SIZE              256
#define OHOS_LFS_PROG_SIZE              256
#define OHOS_LFS_LOOKAHEAD_SIZE         64

/* ============================================================================
 * 4. HDF (Hardware Driver Foundation) Pin Mappings
 * ============================================================================ */
#define HDF_GPIO_PIN_LED_STATUS         2
#define HDF_GPIO_PIN_BUTTON_USER        0
#define HDF_GPIO_PIN_INT_SENSOR         4

#define HDF_UART_PORT_DEFAULT           0
#define HDF_UART_BAUDRATE_DEFAULT       115200
#define HDF_UART_TX_PIN                 1
#define HDF_UART_RX_PIN                 3

#define HDF_I2C_PORT_DEFAULT            0
#define HDF_I2C_SDA_PIN                 21
#define HDF_I2C_SCL_PIN                 22
#define HDF_I2C_SPEED_STANDARD          100000
#define HDF_I2C_SPEED_FAST              400000

#define HDF_SPI_PORT_DEFAULT            2 /* VSPI */
#define HDF_SPI_MOSI_PIN                23
#define HDF_SPI_MISO_PIN                19
#define HDF_SPI_SCLK_PIN                18
#define HDF_SPI_CS_PIN                  5

#define HDF_WDT_TIMEOUT_SECONDS_DEFAULT 2

/* ============================================================================
 * 5. Communication & WiFi / LwIP Stack
 * ============================================================================ */
#define OHOS_WIFI_MAX_AP_SCAN_COUNT     16
#define OHOS_LWIP_TCP_MSS               1460
#define OHOS_LWIP_TCP_SND_BUF           (4 * OHOS_LWIP_TCP_MSS)
#define OHOS_LWIP_TCP_WND               (4 * OHOS_LWIP_TCP_MSS)

/* ============================================================================
 * 6. HUKS Security & Hardware Acceleration
 * ============================================================================ */
#define OHOS_HUKS_HARDWARE_CRYPTO_SUPPORT 1
#define OHOS_HUKS_KEY_STORE_PATH        "/data/huks_keystore"
#define OHOS_HUKS_AES_SUPPORTED         1
#define OHOS_HUKS_SHA256_SUPPORTED      1

/* ============================================================================
 * 7. HiLogLite & System Parameters
 * ============================================================================ */
#define OHOS_HILOG_BUFFER_SIZE          2048
#define OHOS_DEVICE_TYPE                "smartDevice"
#define OHOS_MANUFACTURE                "Espressif / OpenHarmony Community"
#define OHOS_BRAND                      "OpenHarmony"
#define OHOS_MARKET_NAME                "ESP32-DevKitC-OHOS"
#define OHOS_PRODUCT_MODEL              "ESP32-WROOM-32D"
#define OHOS_SOFTWARE_VERSION           "OpenHarmony 4.1.0 Release"
#define OHOS_SDK_API_VERSION            11

#ifdef __cplusplus
}
#endif

#endif /* OHOS_ESP32_CONFIG_H */
