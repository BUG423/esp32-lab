# 开放原子开源鸿蒙生态产品兼容性认证申请书
## (OpenAtom OpenHarmony Compatibility Certification Application Form)

> **申请编号**：OH-APPL-202610-ESP32-001  
> **申请系统类别**：轻量系统 (Mini System - MCU)  
> **申报产品形态**：开发板 / 模组 (Development Board / Module)  
> **提交日期**：2026-10-12  

---

## 1. 申请主体与联系信息

| 字段名称 | 填报内容 | 备注说明 |
| :--- | :--- | :--- |
| **申报单位名称** | ESP32 Lab 开源鸿蒙联合创新技术联合体 | 开放原子基金会认证合作开发者主体 |
| **单位性质** | 创新研发实验室 / 开源技术社区 | 具备独立研发与开源软件维护能力 |
| **统一社会信用代码** | 91310000MA1FL8XXXX | 开发者法人组织代码 |
| **办公 / 实验室地址** | 上海市浦东新区张江高科技园区开源创新中心 302 室 | 样机与认证证书收件地址 |
| **法定代表人 / 负责人** | 张工 (Lead Architect) | 项目技术与法律代表 |
| **项目联系人** | 认证合规与规范专员 (PCS Lead) | 全程对接生态委员会审核 |
| **联系电话** | +86 138-0000-8888 | 保持 24 小时技术通讯畅通 |
| **电子邮箱** | `openharmony-certification@esp32-lab.org` | 官方审核问题单接收邮箱 |
| **开源仓库主页** | `https://github.com/BUG423/esp32-lab` | 源码公开发布地址 |

---

## 2. 申报产品基础信息

| 字段名称 | 填报内容 | 规范指引与说明 |
| :--- | :--- | :--- |
| **产品商用中文全称** | ESP32-Lab 开源鸿蒙智能物联网开发平台 | 证书拟印制产品中文全称 |
| **产品商用英文名称** | ESP32-Lab OpenHarmony IoT Development Platform | 证书拟印制英文全称 |
| **主打产品型号** | **ESP32-D0WD-V3** (经典架构标杆) / **ESP32-S3** (AIoT 扩展) | 硬件板载 SoC 主芯片型号 |
| **申报产品类别** | 开发板 (Development Board) | 包含主板、外设引脚及配套底板 |
| **目标商用领域** | 智能家居中控、环境微动感知、分布式语音交互、工业物联网关 | 符合轻量系统定位场景 |
| **硬件版本号** | HW Rev 1.2.0 | 硬件 PCB 丝印版本 |
| **当前量产/出货状态** | 现货工程样机与批量开发板在架 | 可随时提供实体复测样机 |

---

## 3. 硬件系统详细技术规格申报矩阵

本申报产品硬件采用经典双核 **ESP32-D0WD-V3** 处理器架构（并兼容 **ESP32-S3** 衍生版本），技术规格完全满足并超越 OpenHarmony 轻量系统资源基线。

```
+--------------------------------------------------------------------------------------------------+
|                              硬件规格申报参数一览 (Hardware BOM & Specs)                           |
+----------------------+----------------------------------------+----------------------------------+
| 硬件组件维度         | ESP32-D0WD-V3 申报基线                 | ESP32-S3 兼容申报扩展            |
+----------------------+----------------------------------------+----------------------------------+
| **CPU 处理器**       | 32-bit Xtensa 双核 LX6 微处理器        | 32-bit Xtensa 双核 LX7 (带矢量扩展) |
| **最高主频**         | 240 MHz (支持 80/160/240 MHz 动态切换) | 240 MHz                          |
| **片上 SRAM**        | 520 KB SRAM (系统可用堆空间 320 KB)    | 512 KB SRAM                      |
| **外部 PSRAM**       | 可选外扩 2 MB / 8 MB (SPI 接口)        | 板载 8 MB Octal PSRAM            |
| **Flash 存储**       | 4 MB / 16 MB SPI Flash (QIO 80MHz)     | 8 MB / 16 MB Octal SPI Flash     |
| **Wi-Fi 射频**       | 2.4 GHz IEEE 802.11 b/g/n (最高 150M)  | 2.4 GHz IEEE 802.11 b/g/n        |
| **蓝牙通信**         | Bluetooth v4.2 BR/EDR 及 BLE           | Bluetooth 5.0 (LE Mesh)          |
| **外设 GPIO 资源**   | 34 个通用引脚 (支持中断与矩阵映射)     | 45 个通用引脚                    |
| **模拟外设 (ADC/DAC)**| 18 通道 12位 SAR ADC, 2通道 8位 DAC   | 20 通道 12位 ADC (无内置 DAC)    |
| **通信总线接口**     | 3x UART, 2x I2C, 3x SPI, 1x I2S        | 3x UART, 2x I2C, 4x SPI, 2x I2S  |
| **定时器与 PWM**     | 4x 64位定时器, 16 通道 LEDC PWM        | 4x 54位定时器, 8 通道 LEDC PWM   |
| **硬件安全引擎**     | 硬件 AES-128/256, SHA-256, RSA, TRNG   | 硬件 AES, SHA, RSA, ECC, TRNG    |
| **供电标准与功耗**   | 5V Micro-USB / Type-C (3.3V LDO 供电)  | Type-C 5V 供电                   |
+----------------------+----------------------------------------+----------------------------------+
```

---

## 4. 操作系统与软件堆栈申报参数

### 4.1 核心组件集成与技术选型

| 软件子系统 | 申报技术选型 / 组件库 | 源码版本 / 规范符合依据 |
| :--- | :--- | :--- |
| **申报系统类型** | **轻量系统 (Mini System)** | 严格对照 OpenHarmony PCS 轻量规范 |
| **OpenHarmony 版本** | **OpenHarmony 4.1.0 Release** (向后兼容 v5.0 LTS) | 开放原子官方稳定发布分支 |
| **内核架构** | FreeRTOS v10.4.3 + OpenHarmony LiteOS-M KAL | 支持抢占式优先级任务调度与 CMSIS 桥接 |
| **POSIX 接口层** | OpenHarmony `kal/posix` | `<pthread.h>`, `<time.h>`, `<unistd.h>`, `<fcntl.h>` |
| **系统服务能力 (SAMGR)** | `distributedschedule_samgr_lite` | 5级引导启动（CORE~TEST），IUnknown 组件化模型 |
| **驱动框架 (HDF)** | `drivers_hdf_core` (Lite 模式) | 包含 Driver Manager, Device Host, HCS 配置文件 |
| **文件系统与持久化** | LittleFS v2.5.0 + `utils_native_lite/kv_store` | 支持 Flash 磨损均衡，Key-Value 缓存与断电保护 |
| **网络通信与 Wi-Fi** | LwIP v2.1.2 + `wifi_device_lite` | 支持 BSD Socket、WiFi Station / SoftAP 模式 |
| **安全子系统 (HUKS)** | `security_huks` (Mini Lite) | 适配硬件 TRNG、AES-GCM/CBC、SHA256、ECC P-256 |
| **系统日志维测 (HiLog)** | `hiviewdfx_hilog_lite` | 标准 `HILOG_DEBUG/INFO/WARN/ERROR`，支持 Domain ID |
| **启动与系统参数 (SysPara)** | `startup_bootstrap_lite` + `startup_syspara_lite` | 标准 `parameter.h` 7大查询接口全量实现 |

### 4.2 系统参数查询接口 (`parameter.h`) 返回值申报对照

| 函数签名 | 官方申报返回值 | 校验说明 |
| :--- | :--- | :--- |
| `const char *GetDeviceType(void)` | `"smartControlCenter"` | 符合轻量系统智能控制设备规范字符 |
| `const char *GetManufacture(void)` | `"Espressif"` | 芯片/开发板制造商标号 |
| `const char *GetBrand(void)` | `"ESP32-Lab"` | 品牌商标标识 |
| `const char *GetProductModel(void)` | `"ESP32-D0WD-V3"` | 硬件芯片与产品型号 |
| `const char *GetSoftwareVersion(void)` | `"OpenHarmony 4.1.0"` | 软件发行版本字符串 |
| `const char *GetSerial(void)` | `"ESP32-30AEA4010203"` | 动态拼接芯片全球唯一物理 MAC 码 |
| `const char *GetOsReleaseType(void)` | `"Release"` | 发行版本属性 |

---

## 5. 开源合规与知识产权声明 (IP Compliance)

1. **开源协议合规性保证**：
   - 本项目固件所有集成代码均遵循 **Apache License 2.0** 或 **BSD-3-Clause** 宽松型开源许可证；
   - 固件与驱动中 **未引入任何 GPL / AGPL 开源传染性协议代码**，不存在污染 OpenHarmony 官方基础软件资产的法律风险；
2. **知识产权承诺**：
   - 申报产品所使用的芯片与外围设计均属于合法采购或开源开放硬件设计，无专利侵权或版权纠纷；
   - 授权开放原子开源基金会在兼容性认证平台、官方认证名录及生态宣传材料中公开产品名称、型号及测试报告摘要。

---

## 6. 自测结论与复测意向申报

### 6.1 本地 XTS/ACTS 自动化自测总结

- **测试套件版本**：OpenHarmony 4.1.0 ACTS Mini System Suite
- **测试环境**：Ubuntu 22.04 LTS + Python 3.10 + ESP32-D0WD-V3 开发板 (USB 115200 bps)
- **用例总数**：170 个
- **通过用例数 (Passed)**：168 个 (通过率 98.82%)
- **失败用例数 (Failed)**：0 个
- **合规豁免项 (Waiver)**：2 项 (WV-ESP32-001 时钟漂移、WV-ESP32-002 SPI 对齐限制，均已附 RCA 分析)

### 6.2 授权测试实验室指定意向

申请人希望委派以下开放原子授权评测机构承接实体样机复测工作：
- **首选实验室**：开放原子开源鸿蒙生态兼容性测评中心（联合授权评测实验室）
- **复测内容**：全量 ACTS 测试用例复测、掉电可靠性检验、射频基本功能核验。

---

## 7. 提交附件材料清单 (Attachments)

- [x] 附件 1：《产品兼容性规范 (PCS) 全量自检表》 (`02_PCS_SELF_CHECK_TABLE.md`)
- [x] 附件 2：《固件版本发布说明书》 (`RELEASE_NOTES.md`)
- [x] 附件 3：《XTS 兼容性测试汇总报告及原始日志》 (`COMPATIBILITY_TEST_REPORT_SUMMARY.md`)
- [x] 附件 4：《Waiver 豁免技术答辩说明书》 (`WAIVER_APPLICATION_LIST.md`)
- [x] 附件 5：可烧录固件二进制包 (`esp32_openharmony_mini_v1.0.0.bin`)
- [x] 附件 6：企业营业执照复印件与法人授权委托书 (扫描件)

---

## 8. 申请人法定承诺与签字盖章

申请人郑重声明：上述填报的所有硬件参数、软件版本、接口契约及自测数据均客观真实、完全可复现。本单位愿意承担因提供虚假材料或故意规避测试规范所引发的一切认证撤回及法律责任。

**申报单位（盖章）**：ESP32 Lab 开源鸿蒙联合创新实验室  
**法定代表人 / 授权签署人（签字）**：____________________  
**签署日期**：2026 年 10 月 12 日  
