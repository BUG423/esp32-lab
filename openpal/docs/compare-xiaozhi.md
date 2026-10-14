# OpenPal 与 xiaozhi-esp32

这是一份取舍说明，不是优劣榜。对 xiaozhi-esp32 的描述以其官方仓库和协议文档为准：
[项目说明](https://github.com/78/xiaozhi-esp32)、
[WebSocket 协议](https://github.com/78/xiaozhi-esp32/blob/main/docs/websocket.md)、
[MQTT+UDP 协议](https://github.com/78/xiaozhi-esp32/blob/main/docs/mqtt-udp.md)、
[MCP 用法](https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-usage.md)。

| 维度 | xiaozhi-esp32 | OpenPal |
|---|---|---|
| 目标 | 产品化语音助手与广泛硬件生态 | 小型、可读、便于自托管改造的参考实现 |
| 传输 | WebSocket 或 MQTT+UDP，Opus 流 | 单 WebSocket，PCM16LE 二进制流 |
| 带宽 | Opus 更省带宽 | 实现直观，但 PCM 带宽较高 |
| 唤醒/交互 | 支持离线唤醒等完整交互 | 当前只有 PTT，唤醒词在路线图 |
| 硬件 | 官方仓库维护大量开发板配置 | 当前持续验证经典 ESP32 + 通用 I2S 麦/功放 |
| 服务端 | 可接官方服务，也有社区服务实现 | 仓库自带 FastAPI 服务，默认自行托管 |
| 无硬件调试 | 重点在设备端 | 浏览器与真机共用协议，可直接调试 |
| 情绪 | 支持表情/显示等能力 | 文本规则 + 连续 valence/arousal，影响提示词、LED、语速 |
| MCP | 官方实现标准化 MCP 设备能力交互 | 当前只是 OpenAI function calling，不能称为 MCP |
| 成熟度 | 社区大、能力完整 | 代码量小，但功能、板级验证和运维能力有限 |

## 怎么选

适合 xiaozhi-esp32 的情况：需要成熟成品体验、离线唤醒、Opus、屏幕 UI、广泛板型或
既有社区服务生态。

适合 OpenPal 的情况：需要读懂整条链路、快速改服务端、在局域网自行部署、先用浏览器
验证交互，或者希望用一个尽量小的协议和固件作为实验底座。

两者也可以互相借鉴，但协议并不兼容，不能把 OpenPal 的函数工具描述成 xiaozhi 的 MCP，
也不能把 OpenPal 当前的 PTT/PCM 实现描述成与 xiaozhi 的实时 Opus 链路等价。
