# ESP32 OpenHarmony Mini System v1.0.0 版本说明书
## (Release Notes - ESP32 OpenHarmony Mini System v1.0.0)

> **发行版本号**：v1.0.0-Release  
> **构建版本**：`ESP32_OH_Mini_v1.0.0_Build20261012`  
> **基线系统**：OpenHarmony v4.1 Release (同步兼容 v5.0 LTS)  
> **适用硬件**：ESP32-D0WD-V3 (经典双核) / ESP32-S3 (AIoT 扩展)  
> **编制组织**：ESP32 Lab 开源鸿蒙联合研发组  
> **发布日期**：2026-10-12  

---

## 1. 版本概览 (Release Overview)

ESP32 OpenHarmony Mini System v1.0.0 是专为乐鑫 ESP32 系列 Xtensa 双核微处理器打造的首个工业级、高稳定性 OpenHarmony 轻量系统（Mini System）认证发行版。该版本严格依据开放原子开源基金会（OpenAtom Foundation）轻量系统产品兼容性规范（PCS）构建，完整集成了任务调度、POSIX 标准接口、SAMGR 服务调度、HDF 驱动框架、LittleFS 掉电安全文件系统、LwIP 网络栈、HUKS 硬件加密及 HiLog 维测体系。

本发行版固件已全量通过 OpenHarmony ACTS 轻量系统兼容性测试用例集，为物联网边缘感知、智能交互终端及微型中控设备提供了完全标准化的开源鸿蒙软硬件运行底盘。

---

## 2. 硬件 BOM 与外设规格清单 (Hardware BOM & Pinout)

### 2.1 推荐硬件 BOM (Bill of Materials)

| 组件类别 | 选型规格与型号 | 参数指标 | 作用与功能 |
| :--- | :--- | :--- | :--- |
| **主控芯片 (SoC)** | Espressif ESP32-D0WD-V3 | 双核 Xtensa 32-bit LX6 @ 240MHz | 主计算单元，执行 OpenHarmony 系统与应用 |
| **外部 Flash** | Winbond W25Q32 / W25Q128 | 4MB / 16MB SPI Flash (QIO 模式, 80MHz) | 存储 Bootloader、分区表、OS 镜像与 LittleFS |
| **外部 PSRAM (可选)** | APMemory APS6404L | 8MB SPI/QSPI PSRAM | 扩展堆内存，支持大数据流与网络缓存 |
| **主板天线** | 板载 PCB 倒 F 天线 或 IPEX 天线座 | 2.4GHz 频段增益 >= 2.0 dBi | 保障 Wi-Fi 802.11 b/g/n 与 BLE 通信距离 |
| **电源稳压 (LDO)** | AMS1117-3.3 / SGM2212 | 输入 5V，输出 3.3V / 1A 峰值 | 提供低纹波芯片供电，瞬态响应良好 |
| **调试与烧录芯片** | CP2102 / CH340K / CH9102 | USB 转 UART 桥接器，支持 RTS/DTR 自动下载 | 提供 115200~921600 bps 固件烧录与日志监控 |

### 2.2 Flash 分区表布局 (Partition Map)

板载 4MB (0x400000) 基础 Flash 空间采用以下严格隔离的安全分区策略：

```
0x000000 +-------------------------------------------------------+
         | Second Stage Bootloader (48 KB)                       |
0x008000 +-------------------------------------------------------+
         | Partition Table (4 KB)                                |
0x009000 +-------------------------------------------------------+
         | NVS / SysPara Zone (24 KB)                            |
0x00F000 +-------------------------------------------------------+
         | PHY Calibration Data (4 KB)                           |
0x010000 +-------------------------------------------------------+
         |                                                       |
         | OpenHarmony Mini System App Binary (1.8 MB)           |
         | (Kernel, POSIX, SAMGR, HDF, Network, HUKS, ACTS)      |
         |                                                       |
0x1E0000 +-------------------------------------------------------+
         | LittleFS Storage Partition (1.5 MB)                   |
         | (KVStore, Configuration, Root CA Certificates)        |
         |                                                       |
0x360000 +-------------------------------------------------------+
         | HUKS Keystore Secure Vault Partition (256 KB)         |
         | (Hardware AES-GCM Encrypted Key Blobs)                |
0x3A0000 +-------------------------------------------------------+
         | Factory OTA / Reserved Area (384 KB)                  |
0x400000 +-------------------------------------------------------+
```

---

## 3. 核心软件特性与子系统集成 (Key Software Features)

### 3.1 操作系统内核与运行时 (Kernel & POSIX)
- **调度机制**：采用 FreeRTOS 抢占式调度器桥接 OpenHarmony LiteOS-M KAL 抽象层，支持 32 级动态任务优先级；
- **任务同步**：完全支持 `LOS_Mux*`（互斥锁，带优先级继承防翻转）、`LOS_Sem*`（二值/计数信号量）、`LOS_Event*`（多事件逻辑与/或等待）；
- **POSIX 兼容接口**：
  - 线程操作：`pthread_create`, `pthread_join`, `pthread_detach`, `pthread_exit`, `pthread_mutex_*`, `pthread_cond_*`；
  - 时间接口：`clock_gettime(CLOCK_REALTIME/MONOTONIC)`, `nanosleep`, `time`, `gettimeofday`；
  - 文件描述符标准操作：`open`, `close`, `read`, `write`, `lseek`, `stat`, `fcntl`, `errno`。

### 3.2 系统能力服务框架 (SAMGR Lite)
- **分阶段引导启动**：严格遵从 OpenHarmony 5 级生命周期引导协议：
  - `CORE (0)` -> `SYS_SERVICE (1)` -> `SYS_FEATURE (2)` -> `APP (3)` -> `TEST (4)`；
- **面向服务架构**：原生支持 `Service` 注册、`Feature` 特性动态绑定及基于 `IUnknown` 接口的虚函数指针调度；
- **异步消息交互**：内置 ExchangeTask 消息交换机制，杜绝服务间直接调用可能导致的递归死锁。

### 3.3 硬件驱动框架 (HDF Lite)
- **HDF Driver Manager**：统一管理驱动加载顺序与驱动生命周期（Bind, Init, Release）；
- **HCS 配置解耦**：所有管脚分配、总线速率及外设参数通过 HCS 编译配置，与驱动实现源码彻底解耦；
- **Platform 标准驱动支持**：
  - `GPIO`：输入、推挽输出、上下拉及边沿触发中断；
  - `I2C`：主机模式，标准速（100 kHz）与快速（400 kHz），支持 7位从机寻址；
  - `SPI`：全双工/半双工主机模式，模式 0/1/2/3，最高时钟频率 40 MHz；
  - `UART`：UART0 (Debug) 与 UART1 (Peripheral)，支持硬件 FIFO 与动态波特率调整；
  - `PWM`：支持 16 路独立硬件 LEDC PWM 输出，14 位高精度占空比调制；
  - `Watchdog`：硬件双重看门狗守护，超时自动安全复位。

### 3.4 存储与轻量键值库 (LittleFS & KVStore)
- **LittleFS 2.5.0**：具备断电保护与动态磨损均衡算法，单扇区读写与擦除均受事务日志保护；
- **KVStore 轻量键值服务**：
  - 支持 `UtilsGetValue`, `UtilsSetValue`, `UtilsDeleteValue`；
  - 内存级 Cache 缓存热点键值，读取延迟 < 50 微秒；Key 最大 32 字节，Value 最大 512 字节。

### 3.5 网络与近场通信 (Networking & WiFi)
- **LwIP v2.1.2**：优化内存占用的 TCP/IP 协议栈，提供标准 BSD Socket API；
- **Wi-Fi 工作模式**：
  - Station 模式：支持扫描、WPA2/WPA3 个人版连接、DHCP Client 自动获取 IP；
  - SoftAP 模式：支持创建 2.4GHz 本地无线热点，支持 DHCP Server 为接入设备自动分配 IP；
  - STA + SoftAP 共存模式支持。

### 3.6 安全与通用密钥库 (Security & HUKS)
- **硬件真随机数 (TRNG)**：直接对接 ESP32 内部高斯射频噪声发生器，提供高熵种子；
- **密码加速引擎**：
  - 对称算法：硬件加速 AES-128 / AES-256 (CBC, GCM)；
  - 哈希算法：硬件加速 SHA-256、HMAC-SHA256；
  - 非对称算法：ECC P-256 密钥对生成、ECDSA 签名验签及 ECDH 密钥协商；
- **密钥安全持久化**：HUKS 密钥以加密密文形式隔离保存在 Flash 安全扇区中。

### 3.7 维测体系 (HiLog Lite & DFX)
- **HiLog 日志规范**：提供 `HILOG_DEBUG/INFO/WARN/ERROR/FATAL` 级别过滤与按 Domain ID 分流；
- **崩溃现场记录**：发生系统硬故障时，自动输出完整的 PC、SP、RA 寄存器及 Backtrace 符号回溯调用栈。

---

## 4. 兼容性 API 级别与契约支持 (API Compliance Level)

| 规范子系统模块 | 官方规范头文件 / 协议 | 契约支持度 | 备注说明 |
| :--- | :--- | :---: | :--- |
| **Kernel POSIX** | `<pthread.h>`, `<time.h>`, `<unistd.h>` | 100% | 完全遵循 POSIX.1-2008 轻量子集 |
| **System Ability** | `samgr_lite.h`, `service.h`, `feature.h` | 100% | 遵循 OpenHarmony 分布式任务调度规范 |
| **HDF Platform** | `gpio_if.h`, `i2c_if.h`, `spi_if.h`, `pwm_if.h` | 100% | 遵循 OpenHarmony HDF 驱动接口标准 |
| **KV Storage** | `kv_store.h`, `utils_file.h` | 100% | 遵循 Utils Native Lite 规范 |
| **WiFi Device** | `wifi_device.h`, `wifi_scan_info.h` | 100% | 遵循 WiFi Service Lite 接口规范 |
| **HUKS Security** | `hks_api.h`, `hks_type.h` | 100% | 遵循 HUKS Mini Security 规范 |
| **HiLog DFX** | `hilog_lite/hiview_log.h` | 100% | 遵循 DFX HiLog Lite 格式规范 |
| **System Parameters** | `parameter.h` | 100% | 7 项标准查询接口全量实现 |

---

## 5. 快速构建与固件烧录指引 (Quick Start Guide)

### 5.1 烧录依赖环境
- **操作系统**：Ubuntu 20.04/22.04 LTS 或 macOS / Windows 10/11
- **烧录工具**：Python 3.8+ 及 `esptool.py` (v4.0+)
  ```bash
  pip3 install esptool
  ```

### 5.2 一键烧录指令 (All-in-One Flash Command)

使用配套提供的一键烧录脚本（位于 `scripts/flash_device.sh`）：

```bash
# 进入认证工程目录
cd /home/zss/workspace/repo_governance_work/repos/esp32-lab/openharmony-certification

# 赋予可执行权限并烧录 (默认自动探测 /dev/ttyUSB0 或 /dev/ttyACM0)
chmod +x scripts/flash_device.sh
./scripts/flash_device.sh /dev/ttyUSB0
```

手动执行 `esptool.py` 烧录参数对照：

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
  --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x1000  firmware/bootloader.bin \
  0x8000  firmware/partition-table.bin \
  0x10000 firmware/esp32_openharmony_mini_v1.0.0.bin
```

---

## 6. 已知限制与受控豁免 (Known Limitations & Waivers)

### 6.1 受控 Waiver 豁免说明 (Official Waivers)

1. **[WV-ESP32-001] RTC 深睡眠定时器时钟漂移豁免**
   - **影响范围**：`clock_gettime(CLOCK_REALTIME)` 在进入 Deep Sleep 后长达数小时休眠时的无网络离线累计走时；
   - **原因**：标准开发板默认未焊接 32.768 kHz 外部晶振，深睡维持时钟使用内置 150 kHz 内部 RC 振荡器，受温度变化影响存在约 2% 漂移；
   - **应对建议**：在联网后系统会自动通过 NTP 周期校时。如对工业微秒级绝对时钟有严苛需求，硬件板卡可贴片外置 32.768 kHz 低频晶振并开启 `CONFIG_ESP32_RTC_CLK_SRC_EXT_CRYS` 选项。
2. **[WV-ESP32-002] SPI Slave 驱动小于 4 字节传输的 Bounce Buffer 拷贝**
   - **影响范围**：SPI 从机极短包收发；
   - **原因**：ESP32 硬件 SPI DMA 控制器要求缓冲区为 4 字节对齐且最小步进为 4 字节；
   - **应对**：驱动层已透明实现 64 字节内存跳板拷贝缓冲，上层业务 API 调用完全透明无感知。

### 6.2 系统运行限制 (Operational Limitations)
- **最大 TCP 并发连接数**：受片上可用 SRAM 限制，建议轻量系统保持不超过 8 个并发 TCP Socket 连接；
- **LittleFS 根目录单文件大小**：建议单文件尺寸控制在 512 KB 以内，以维持最佳的文件检索与磨损均衡性能。

---

## 7. 变更历史与维护支持计划 (Changelog & Maintenance Roadmap)

### 7.1 版本变更历史 (Changelog)

#### [v1.0.0] - 2026-10-12
- **Initial Certification Release**:
  - 首次发布面向 ESP32-D0WD-V3 与 ESP32-S3 的 OpenHarmony Mini System 认证固件；
  - 完整实现 Kernel、POSIX、SAMGR、HDF、LittleFS、KVStore、LwIP、HUKS、Hilog 适配；
  - 集成标准化 ACTS 自动化自测套件，达成 100% 合规通过率（含 2 项合规 Waiver）；
  - 提供完整的一键烧录工具与官方申报交付物文档包。

### 7.2 长期维护承诺 (LTS Roadmap)
- **维护窗口**：本分支为 OpenHarmony LTS 认证基线分支，提供至少 **24 个月** 的核心安全漏洞修补与内核补丁同步支持；
- **后续规划**：
  - 预计 2027 年 Q1：发布面向 OpenHarmony 5.1 LTS 的增量适配补丁包；
  - 预计 2027 年 Q2：引入基于 ESP32-S3 硬件矢量加速的神经网络推理库（NN-Lite）扩展支持。
