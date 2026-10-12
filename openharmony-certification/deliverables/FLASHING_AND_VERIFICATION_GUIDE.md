# ⚡ ESP32 开源鸿蒙（OpenHarmony）轻量系统固件烧录与验证交付指南
# Flashing & Board-Level Verification Guide for OpenHarmony Mini System on ESP32

---

## 📖 1. 概述与硬件环境准备

本指南面向 ESP32 芯片的 OpenHarmony 轻量系统（Mini System / LiteOS-M 内核）基线固件交付，提供完整的工程镜像烧录、串口终端监控配置、板级启动自检与 XTS（X Test Suite）兼容性认证验证流程。

### 1.1 推荐硬件规格
- **主控芯片**：ESP32-D0WD-V3 (Xtensa Dual-Core 32-bit LX6, 240MHz)
- **典型开发板**：NodeMCU-32S / ESP32-WROOM-32 / ESP32 DevKitC v4
- **板载存储**：4MB SPI Flash (DIO 模式, 40MHz)
- **供电与通信**：Micro-USB / Type-C 数据线（具备 CP2102 / CH340 / FT232 串口转 USB 芯片）
- **外设指示**：板载 GPIO2 用户 LED（用于 HDF GPIO 驱动自检）

---

## 🗺️ 2. Flash 分区表与物理偏移量映射

OpenHarmony Mini System 专属定制分区表（Flash 总容量：4MB / 0x400000）：

| 分区名称 | 类型 (Type) | 子类型 (SubType) | 物理起始偏移量 (Offset) | 镜像大小 (Size) | 作用描述 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **bootloader** | System | Bootloader | `0x1000` | 28KB (`0x7000`) | ESP32 二级引导装载程序 |
| **partition-table** | System | Partition Table | `0x8000` | 4KB (`0x1000`) | 芯片分区定义表 |
| **nvs** | Data | NVS | `0x9000` | 20KB (`0x5000`) | 轻量级 NVS 存储 |
| **otadata** | Data | OTA | `0xe000` | 8KB (`0x2000`) | OTA 升级状态标记槽位 |
| **phy_init** | Data | PHY | `0x10000` | 4KB (`0x1000`) | RF 物理层射频校准数据 |
| **factory** (App) | App | Factory | `0x20000` | 1920KB (`0x1E0000`) | OpenHarmony 认证基线固件主体 |
| **littlefs_storage** | Data | SPIFFS / LFS | `0x200000` | 1024KB (`0x100000`) | LittleFS 文件系统持久化存储 |
| **huks_keystore** | Data | 0x84 (HUKS) | `0x300000` | 128KB (`0x20000`) | HUKS 硬件根密钥与凭据存储区 |

---

## 🚀 3. 固件烧录操作指南

### 3.1 交付归档包结构
在 `deliverables/` 目录下提供打包生成的发布文件：
```text
deliverables/
├── SHA256SUMS                                      # 镜像校验和文件
├── openharmony_esp32_firmware_v1.0.0.tar.gz        # tar 归档压缩包
├── openharmony_esp32_firmware_v1.0.0.zip           # zip 归档压缩包
└── openharmony_esp32_firmware_v1.0.0/              # 解压产物目录
    ├── bootloader.bin                              # 0x1000 引导镜像
    ├── partition-table.bin                         # 0x8000 分区表镜像
    ├── ota_data_initial.bin                        # 0xe000 OTA 数据镜像
    ├── openharmony_cert_firmware.bin               # 0x20000 鸿蒙主固件
    ├── flash_args                                  # esptool 参数文件
    └── flash_firmware.sh                           # 一键烧录执行脚本
```

### 3.2 方式一：使用发布包一键脚本烧录（推荐）
```bash
# 解压交付包（如已处于工程目录可直接进入解压目录）
cd openharmony-certification/deliverables/openharmony_esp32_firmware_v1.0.0

# 赋予执行权限并烧录 (默认端口 /dev/ttyUSB0, 波特率 460800)
chmod +x flash_firmware.sh
./flash_firmware.sh /dev/ttyUSB0 460800
```

### 3.3 方式二：使用 esptool.py 手动多分区烧录
```bash
# 安装 esptool 工具（如未安装）
pip3 install esptool

# 执行全镜像烧录
python3 -m esptool --chip esp32 -p /dev/ttyUSB0 -b 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_size 4MB --flash_freq 40m \
    0x1000 bootloader.bin \
    0x8000 partition-table.bin \
    0xe000 ota_data_initial.bin \
    0x20000 openharmony_cert_firmware.bin
```

### 3.4 方式三：使用 ESP-IDF 工程一键烧录
在源码工程路径 `openharmony-certification/firmware/` 下直接执行：
```bash
idf.py -p /dev/ttyUSB0 flash
```

---

## 🖥️ 4. 串口终端监控与通信配置

### 4.1 串口参数规格
- **波特率 (Baud rate)**: `115200`
- **数据位 (Data bits)**: `8`
- **停止位 (Stop bits)**: `1`
- **校验位 (Parity)**: `None (无)`
- **流控 (Flow control)**: `None (关)`

### 4.2 串口监控工具
使用 `idf.py monitor` 或系统终端工具：
```bash
# 方式 A: idf.py monitor (推荐，支持符号解析与快捷复位)
idf.py -p /dev/ttyUSB0 monitor

# 方式 B: minicom
minicom -D /dev/ttyUSB0 -b 115200

# 方式 C: picocom
picocom -b 115200 /dev/ttyUSB0
```

---

## 🔍 5. 板级启动自检与验证指南

ESP32 开发板通电或按键复位（EN / RST）后，系统将依次执行 OpenHarmony 六大核心启动序列：

### 5.1 启动日志对照检查清单 (Verification Checklist)

1. **OpenHarmony Banner 打印**：
   - 终端显示 ASCII 艺术字符横幅及固件版本号 `v1.0.0-OpenHarmony-3.2-Mini`。
2. **Boot Step 1: LiteOS-M 调度器启动**：
   - `[Boot Step 1/6] Starting OpenHarmony LiteOS-M kernel & scheduler abstraction...`
   - `LiteOS-M Kernel initialized (Tick: 1000Hz, Max Priorities: 32)`
3. **Boot Step 2: KVStore 存储引擎初始化**：
   - `[Boot Step 2/6] Initializing KVStore Lite engine backed by Flash/NVS...`
   - `KVStore Lite engine initialized successfully`
4. **Boot Step 3: SAMGR 微内核服务框架加载**：
   - `[Boot Step 3/6] Initializing SAMGR microkernel service communication framework...`
   - `SAMGR microkernel service framework bootstrap initialized`
   - `Registered service: SysParamService (id: 0)`
5. **Boot Step 4: HDF 硬件抽象驱动装载**：
   - `[Boot Step 4/6] Loading HDF platform driver foundation (GPIO, UART, I2C, SPI)...`
   - `HDF DeviceManager initializing platform hardware drivers...`
   - `Driver [HDF_PLATFORM_GPIO] bound and initialized`
   - `Driver [HDF_PLATFORM_UART] bound and initialized`
   - `Driver [HDF_PLATFORM_I2C] bound and initialized`
   - `Driver [HDF_PLATFORM_SPI] bound and initialized`
6. **Boot Step 5: LittleFS 文件系统挂载**：
   - `[Boot Step 5/6] Mounting LittleFS persistent file system storage...`
   - `Storage partition mounted successfully: Total 1024 KB`
   - `Storage self-test passed: Created /storage/boot.txt`
7. **Boot Step 6: WiFi 网络就绪**：
   - `[Boot Step 6/6] Bringing up OpenHarmony WiFi network stack...`
   - `WiFi subsystem online! Station MAC: XX:XX:XX:XX:XX:XX`
8. **启动 XTS 自动化测试就绪服务**：
   - `XTS Automated Test Agent Service started and ready for commands`
   - 终端输出就绪指令行：`+XTS:READY,KERNEL:OK,SAMGR:OK,HDF:OK,STORAGE:OK,WIFI:OK`

---

## 🧪 6. XTS 自动化测试与交互命令验证

固件启动后，XTS 测试代理在串口终端持续监听交互式 AT 控制协议，可输入以下测试指令：

| 发送指令 (AT Command) | 响应格式 (Response) | 说明 |
| :--- | :--- | :--- |
| `AT+XTS=PING` | `+XTS:PONG` | 链路连通性握手测试 |
| `AT+XTS=VERSION` | `+XTS:VERSION,v1.0.0-OpenHarmony-3.2-Mini` | 查询固件鸿蒙版本与架构 |
| `AT+XTS=STATUS` | `+XTS:STATUS,READY,HEAP:<bytes>` | 查询当前就绪状态与空闲堆内存 |
| `AT+XTS=RUN,kernel_lite` | `[XTS_RESULT] {"subsystem": "kernel_lite", "passed": 12, "failed": 0, "status": "PASSED"}` | 触发内核调度与 IPC 自检套件 |
| `AT+XTS=RUN,samgr_lite` | `[XTS_RESULT] {"subsystem": "samgr_lite", "passed": 3, "failed": 0, "status": "PASSED"}` | 触发 SAMGR 服务通信自检套件 |
| `AT+XTS=RUN,hdf_drivers` | `[XTS_RESULT] {"subsystem": "hdf_drivers", "passed": 5, "failed": 0, "status": "PASSED"}` | 触发 HDF GPIO/UART/I2C/SPI 外设驱动自检 |
| `AT+XTS=RUN,littlefs_kvstore`| `[XTS_RESULT] {"subsystem": "littlefs_kvstore", "passed": 3, "failed": 0, "status": "PASSED"}` | 触发 KV 键值与文件系统读写自检 |
| `AT+XTS=RUN,ALL` | 输出各子系统结果并给出 `[XTS_SUITE_COMPLETE]` | 执行全量集成认证测试套件 |

---

## 🛠️ 7. 常见故障排查 (Troubleshooting FAQ)

1. **烧录时提示 `Failed to connect to ESP32: Timed out waiting for packet header`**：
   - 检查串口设备名是否正确（如 macOS 为 `/dev/cu.usbserial-*`，Linux 为 `/dev/ttyUSB0`）。
   - 在 esptool 连接阶段（显示 `Connecting.....` 时），按住开发板上的 **BOOT**（IO0）按键，直到握手成功开始烧录。
2. **串口输出乱码**：
   - 请确认终端波特率已设置为 `115200`，ESP32 启动阶段 ROM 日志与固件 HiLog 均统一工作于 115200 波特率。
3. **LittleFS 挂载失败**：
   - 固件支持首次启动时自动格式化 `littlefs_storage` 分区；若分区表被意外修改，可通过 `esptool.py erase_flash` 清空整片 Flash 后重新完整烧录。
