# ESP32 OpenHarmony Mini System 固件烧录与环境准备指南
## (Firmware Flashing & Hardware Setup Guide)

> **文档标识**：OH-FLASH-GUIDE-ESP32-01  
> **适用硬件**：ESP32-D0WD-V3 / ESP32-S3 开发板  
> **固件版本**：v1.0.0-Release  
> **最新修订**：2026-10-12  

---

## 1. 硬件连接与准备

1. **开发板物理连接**：
   - 使用优质 Micro-USB 或 Type-C 数据线将 ESP32 开发板连接至主机 USB 端口；
   - Linux 环境下默认设备节点通常为 `/dev/ttyUSB0` 或 `/dev/ttyACM0`；
   - macOS 环境下设备节点通常为 `/dev/cu.usbserial-*` 或 `/dev/cu.wchusbserial*`；
   - Windows 环境下为 `COMx`（通过设备管理器查看）。
2. **串口权限配置 (仅 Linux 需执行一次)**：
   ```bash
   sudo usermod -a -G dialout $USER
   # 重启或重新登录生效，临时授权可执行：
   sudo chmod 666 /dev/ttyUSB0
   ```

---

## 2. 软件工具准备

推荐使用官方标准的 Python 烧录工具 `esptool.py`：

```bash
# 安装或更新 esptool
pip3 install --upgrade esptool
```

验证安装：
```bash
esptool.py version
```

---

## 3. 固件组成与烧录地址映射表 (Memory Flash Offset)

| 分区 / 镜像文件名 | Flash 烧录起始地址 | 作用与内容 |
| :--- | :--- | :--- |
| `bootloader.bin` | `0x1000` | 二级 Bootloader，负责芯片初始化与分区表引导 |
| `partition-table.bin` | `0x8000` | Flash 分区表二进制定义 |
| `esp32_openharmony_mini_v1.0.0.bin` | `0x10000` | OpenHarmony 轻量系统全功能核心固件 |
| `littlefs.bin` *(可选)* | `0x1E0000` | 初始文件系统镜像 (包含系统参数与自检测试资源) |

---

## 4. 推荐烧录方法

### 4.1 方法一：使用自动化一键脚本 (推荐)

在仓库的 `openharmony-certification/` 目录下直接运行：

```bash
# 自动检测端口并烧录
./scripts/flash_device.sh

# 或手动指定串口端口
./scripts/flash_device.sh /dev/ttyUSB0
```

### 4.2 方法二：手动命令行烧录 (分立镜像)

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
    --before default_reset --after hard_reset write_flash -z \
    --flash_mode dio --flash_freq 80m --flash_size 4MB \
    0x1000  firmware/bootloader.bin \
    0x8000  firmware/partition-table.bin \
    0x10000 firmware/esp32_openharmony_mini_v1.0.0.bin
```

### 4.3 彻底擦除整片 Flash (重新配置环境时推荐)

如果开发板此前烧录过其他固件（如 MicroPython 或 Arduino），建议先进行全盘擦除：

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash
```

---

## 5. 启动验证与串口监视 (Verify Boot & Log Monitor)

烧录完成后，可通过串口终端观察 OpenHarmony 引导日志：

```bash
# 使用 minicom / screen 或 idf.py monitor
minicom -D /dev/ttyUSB0 -b 115200
```

期望输出正常引导特征日志：
```text
I (120) boot: OpenHarmony Mini System on ESP32 Dual Core
I (145) samgr: SAMGR_Bootstrap started (Priority 0..4)
I (180) hdf: HDF Driver Framework Lite initialized successfully
I (210) littlefs: LittleFS mounted at /littlefs, total: 1536 KB, free: 1488 KB
I (240) kvstore: KVStore Service initialized, cache capacity: 64 entries
I (270) lwip: LwIP 2.1.2 initialized, WiFi STA initialized
I (310) huks: HUKS Core initialized with HW TRNG engine
I (350) app_main: OpenHarmony 4.1.0 Ready. Running XTS ACTS compatibility tests...
```
