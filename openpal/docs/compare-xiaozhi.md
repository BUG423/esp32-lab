# OpenPal vs 小智 (xiaozhi-esp32)

> 两个项目都值得用。这篇文档说明**取舍差异**，不是拉踩。

## 一句话

| | 定位 |
|---|---|
| **xiaozhi-esp32** | 产品化的开源 AI 语音助手，开箱即用，生态巨大（30k+ star） |
| **OpenPal** | 可拆解、可自托管的 AI 语音**搭子**，强调本地优先 + 情绪交互 |

## 详细对比

| 维度 | xiaozhi-esp32 | OpenPal |
|---|---|---|
| **GitHub 热度** | ~30.5k ⭐ | 新项目 |
| **默认服务端** | xiaozhi.me 官方云 | **自托管 Python server** |
| **离线能力** | 唤醒词离线，对话需联网 | LLM/ASR/TTS **均可本地**（Ollama/whisper/piper） |
| **LLM 接入** | Qwen / DeepSeek 等 | 任意 **OpenAI 兼容** API（含 Ollama） |
| **语音协议** | WebSocket / MQTT+UDP，Opus | WebSocket，JSON + PCM16（v1） |
| **唤醒词** | ✅ ESP-SR，可定制 | 🔜 路线图（PTT 先行） |
| **情绪/人格** | 表情贴图 | **连续情绪模型** + LED + TTS 语速 |
| **无硬件体验** | 需板子 + 配网 | **浏览器麦克风直接玩** |
| **MCP** | ✅ 设备端 + 云端 MCP | 🔜 路线图（简单工具调用） |
| **屏幕表情** | ✅ OLED/LCD emoji | 🔜（先做 LED 灯环） |
| **硬件矩阵** | 138+ 板型 | 通用 ESP32 + I2S 麦/喇叭 |
| **ESP-IDF** | **v6.0.1+**（5.x 不支持） | **v5.5** 即可 |
| **协议文档** | 有，较复杂 | 极简，本文档即协议 |
| **代码规模** | 大而全 | 小而清晰，适合二次开发 |

## 什么时候选谁

**选小智，如果你想：**
- 马上要一个能用的成品
- 需要大量现成板型支持
- 用 MCP 控制智能家居/电脑
- 跟随大社区

**选 OpenPal，如果你想：**
- 数据不出门 / 纯局域网
- 自己改 LLM / TTS / 人格逻辑
- 先在浏览器验证想法再上硬件
- 把「情绪化 AI 伙伴」做成自己的产品/作品
- 用 ESP-IDF 5.5，不想立刻升 6.x

## 协议层差异

### 小智
- 设备 ↔ 服务器自有协议（WebSocket / MQTT+UDP）
- Opus 音频流，双向实时
- 后端可替换（社区有 Python/Java/Go 实现）

### OpenPal
- 单一 WebSocket `/ws`
- 文本帧 JSON 控制 + 二进制帧 PCM16
- 音频回复 base64 内嵌在 JSON（v1 简化；v2 改二进制 + Opus）

```json
// OpenPal 回复
{
  "type": "reply",
  "text": "你好呀！",
  "emotion": {"emotion":"happy","valence":0.9,"arousal":0.7,"color":"#2ee6a8","emoji":"😄"},
  "audio_b64": "<pcm16 base64>",
  "sample_rate": 24000
}
```

## 情绪引擎对比

| | 小智 | OpenPal |
|---|---|---|
| 情绪来源 | 预置表情资源 | 用户话语实时打分 |
| 状态 | 离散表情 ID | 连续 (valence, arousal) |
| 影响范围 | 屏幕贴图 | 提示词 + TTS 语速 + LED 色 |
| 可解释性 | 美术资源 | 同一状态机可导出 JSON |

## 可以一起用

有意思的是两者**不互斥**：

- 用小智的设备端 + 自建后端（社区 server 已有）
- 用 OpenPal 的情绪引擎作为独立服务，给任何语音助手加「性格」
- 把 OpenPal 协议做成小智的 MCP tool provider

---

**结论：** 小智是这个赛道的「安卓」——生态成熟；OpenPal 想做「可刷机的开源固件」——自由度高。
先玩小智建立直觉，再用 OpenPal 改出你自己的版本。
