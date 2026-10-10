# 🤝 OpenPal

**开源 · 本地优先的 AI 语音搭子 —— 可自托管的「小智」替代方案。**

XiaoZhi 火了，但它绑定了官方云、上手链路长。OpenPal 把同一套体验拆开、做透明，
并且加上小智没有的三件事：**完全本地可跑、情绪引擎、浏览器即设备**。

```
 🎙️ 你说话  ──►  OpenPal Server  ──►  🔊 有情绪的语音回复
                  ASR → LLM → TTS
                  + 人格 / 情绪引擎
```

[中文文档](./README_zh.md) · [架构](./docs/architecture.md) · [与小智对比](./docs/compare-xiaozhi.md)

---

## ✨ 为什么是 OpenPal

| | 小智 xiaozhi-esp32 | **OpenPal** |
|---|---|---|
| 服务端 | 官方云（xiaozhi.me）为主 | **完全自托管**，本地 LLM/TTS 可选 |
| 上手 | ESP-IDF 6.x + 复杂配置 | ESP-IDF 5.5 + 浏览器 5 分钟体验 |
| 情绪 | 表情贴图 | **连续情绪模型**（valence/arousal）+ LED + 语气 |
| 无硬件体验 | 需要板子 | **浏览器麦克风直接玩** |
| LLM | 主要对接国内大模型 | 任意 OpenAI 兼容 API / Ollama / DeepSeek / Qwen |
| 协议 | 自有 WebSocket/MQTT 协议 | 极简 JSON + PCM WebSocket，文档全公开 |
| 隐私 | 依赖云端 | **语音可不出局域网** |

---

## 🚀 5 分钟跑起来（不用买硬件）

### 1. 启动服务端

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# 可选：配置 LLM（不配也能用 toy-brain demo）
export PAL_LLM_API_KEY=sk-...
export PAL_LLM_BASE_URL=https://api.deepseek.com/v1   # 或 Ollama / 任意兼容端

python -m pal_server.main
```

### 2. 打开控制台

```
http://localhost:8080
```

- 直接打字聊天
- 点 **🎙️ 按住说话** —— 浏览器麦克风 → ASR → LLM → TTS 语音闭环
- 右侧实时看**情绪引擎**（愉悦度 / 唤醒度 / LED 色）

> 没配 API Key 也能跑：会进入 `toy-brain demo` 模式，有基础对话 + edge-tts 语音。

---

## 🧰 硬件（可选，真机形态）

| 部件 | 型号 | 价格 |
|---|---|---|
| 主控 | ESP32 / ESP32-S3 DevKit | ¥15–30 |
| 麦克风 | INMP441 I2S | ¥8 |
| 功放喇叭 | MAX98357A + 小喇叭 | ¥10 |
| 按键 | 轻触开关（PTT） | ¥1 |
| （可选）LED 灯环 | WS2812 | ¥10 |

总 BOM **< ¥50**。

### 烧录固件

```bash
cd firmware
idf.py set-target esp32      # 或 esp32s3
idf.py menuconfig            # 配 WiFi + 服务器地址 ws://<PC-IP>:8080/ws
idf.py build flash monitor
```

**按住 PTT 说话 → 松开 → 自动识别并语音回复。**

---

## 🧠 情绪引擎（OpenPal 独有）

不只是「换个表情包」——是一套连续情绪状态机：

```
用户话语 ──► 情绪打分 ──► EMA 平滑 ──► (valence, arousal)
                                │
                ┌───────────────┼───────────────┐
                ▼               ▼               ▼
           系统提示词       LED 颜色         TTS 语速
           （语气变化）    （设备灯环）     （快/慢/轻柔）
```

- 开心 → 语速加快、灯变绿
- 难过 → 语气温柔、灯变蓝紫
- 兴奋 → 能量感强、灯变黄

点控制台的**光球**可随时重置情绪。

---

## 📁 目录

```
openpal/
├── server/                 Python 服务端
│   ├── pal_server/
│   │   ├── main.py         FastAPI + WebSocket 主循环
│   │   ├── personality.py  情绪/人格引擎
│   │   ├── asr.py          Whisper 兼容 ASR
│   │   ├── llm.py          OpenAI 兼容 LLM + toy-brain
│   │   └── tts.py          edge-tts / OpenAI TTS
│   └── static/index.html   控制台 + 浏览器语音模式
├── firmware/               ESP-IDF 固件（I2S + WS + PTT）
└── docs/
    ├── architecture.md
    └── compare-xiaozhi.md
```

---

## 🔌 接入任意 LLM

```bash
# DeepSeek
export PAL_LLM_BASE_URL=https://api.deepseek.com/v1
export PAL_LLM_API_KEY=sk-xxx
export PAL_LLM_MODEL=deepseek-chat

# 本地 Ollama
export PAL_LLM_BASE_URL=http://127.0.0.1:11434/v1
export PAL_LLM_API_KEY=ollama
export PAL_LLM_MODEL=qwen2.5:7b

# Qwen / OpenAI / 任意兼容端
export PAL_LLM_BASE_URL=https://dashscope.aliyuncs.com/compatible-mode/v1
```

---

## 🗺️ 路线图

- [x] 浏览器语音闭环（无需硬件）
- [x] 情绪引擎（valence/arousal + LED + TTS rate）
- [x] ESP32 PTT 对讲固件
- [x] 本地/云端 LLM 热切换
- [ ] 唤醒词（ESP-SR / OpenWakeWord）
- [ ] Opus 编解码，降低带宽
- [ ] MCP 工具调用（智能家居 / PC 控制）
- [ ] 多模态：摄像头表情 / 屏幕 emoji
- [ ] OTA 升级
- [ ] Home Assistant 集成

---

## 🤝 和小智的关系

OpenPal **不是** fork，也不是替代品的替代品——是同一赛道的**另一种取舍**：

- 小智：产品化程度高、生态大、开箱即用（用官方云）
- OpenPal：**可拆解、可自托管、可改造**，适合想搞懂每一层、或要求数据不出门的人

两个都值得 star。想直接上手成品，去玩小智；想自己长出一个属于自己的 AI 搭子，来 OpenPal。

---

## License

MIT
