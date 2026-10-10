# 架构说明

## 总览

```
┌──────────────────┐         WebSocket (JSON + PCM)        ┌─────────────────────────┐
│   客户端          │◄────────────────────────────────────►│   OpenPal Server        │
│                  │                                       │                         │
│  · 浏览器麦克风   │   {"type":"text"} / binary PCM        │   personality.py        │
│  · ESP32 PTT     │   {"type":"reply","audio_b64":...}    │     │  情绪引擎          │
│  · 未来：唤醒词   │   {"type":"emotion", valence, ...}    │     ▼                   │
└──────────────────┘                                       │   llm.py  ──► 任意 LLM  │
                                                           │   asr.py  ──► Whisper   │
                                                           │   tts.py  ──► edge-tts  │
                                                           └─────────────────────────┘
```

## WebSocket 协议

### 客户端 → 服务端

| 消息 | 说明 |
|---|---|
| `{"type":"text","text":"..."}` | 文本输入（无音频） |
| binary frame | 原始 PCM16 mono 16kHz 音频块 |
| `{"type":"audio_end"}` | 一段话结束，触发识别 |
| `{"type":"interrupt"}` | 打断当前播放 |
| `{"type":"playback_done"}` | 设备播放完毕 |
| `{"type":"ping"}` | 心跳 |
| `{"type":"recalibrate_mood"}` | 重置情绪 |

### 服务端 → 客户端

| 消息 | 说明 |
|---|---|
| `{"type":"hello",...}` | 握手 + 能力声明 |
| `{"type":"status","state":"listening\|thinking\|speaking"}` | 状态机 |
| `{"type":"emotion","emotion":"happy","valence":0.9,...}` | 情绪快照 |
| `{"type":"transcript","role":"user\|assistant","text":"..."}` | 字幕 |
| `{"type":"reply","text","audio_b64","sample_rate"}` | 回复文本 + TTS 音频 |

## 对话管线

```
PCM16 ──► ASR ──► text ──► PersonalityEngine.observe_user()
                              │
                              ▼
                    system_prompt() 组装
                              │
                              ▼
                    LLM.chat(messages)
                              │
                              ▼
                    TTS.synthesize(text, rate=hints)
                              │
                              ▼
                    broadcast(reply + emotion)
```

## 情绪引擎

状态：`(emotion, valence: 0..1, arousal: 0..1)`

- **更新**：对用户文本做关键词/规则打分 → EMA 平滑，避免跳变
- **影响**：
  1. `system_prompt()` 注入当前情绪指令
  2. `tts_hints()` 映射到语速（arousal 高 → 语速快）
  3. `device_payload()` 下发 LED 色 + emoji

## 固件音频路径

```
INMP441 (I2S RX) ──► ring buffer ──► PTT 松开 ──► WS binary ──► server
server reply  ──► WS JSON audio_b64 ──► base64 decode ──► I2S TX ──► MAX98357A
```

## 部署形态

| 形态 | 说明 |
|---|---|
| 笔记本 demo | server + 浏览器麦克风，零硬件 |
| 桌面搭子 | ESP32 + 麦 + 喇叭，按住说话 |
| 家庭网关 | server 跑在 NAS / 树莓派，全屋多个 ESP32 节点 |
| 纯本地 | Ollama + faster-whisper + piper，完全离线 |

## 安全与隐私

- 服务端默认监听局域网；公网暴露请自行加反代 + TLS + 鉴权
- API Key 仅存在服务端环境变量，不下发设备
- 浏览器语音模式的音频只发到你配置的服务端
