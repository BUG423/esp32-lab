# 架构与协议

## 数据流

```text
浏览器 / ESP32
  │  JSON control + binary PCM16LE
  ▼
FastAPI WebSocket session
  ├─ ASR ──► transcript
  ├─ PersonalityEngine ──► mood + system prompt
  ├─ LLM ──► reply / function calls
  └─ TTS ──► streamed PCM16LE
```

每条 WebSocket 连接拥有独立的：对话历史、情绪状态、录音缓冲和活动任务。全局历史仅供
管理审计使用，不会送进其他客户端的 LLM 上下文。历史接口默认关闭；函数工具事件与设备
命令也只发给调用会话。

## WebSocket v2

服务端连接后先发送：

```json
{
  "type": "hello",
  "protocol": 2,
  "session_id": "...",
  "audio_up": {"format": "pcm_s16le", "sample_rate": 16000},
  "audio_down": {"format": "pcm_s16le", "sample_rate": 24000},
  "max_utterance_ms": 15000,
  "asr_ready": true,
  "tts_ready": true
}
```

### 客户端上行

一次语音输入必须有明确边界：

```text
text:   {"type":"audio_start","format":"pcm_s16le","sample_rate":16000}
binary: <PCM chunk 1>
binary: <PCM chunk 2>
text:   {"type":"audio_end"}
```

服务端限制采样率为 8–48 kHz，按声明采样率和 `PAL_MAX_UTTERANCE_MS` 限制字节数，
并在 ASR 前检查最短时长与 RMS 能量。

其他上行控制消息：

| 消息 | 作用 |
|---|---|
| `{"type":"text","text":"..."}` | 文字对话 |
| `{"type":"interrupt"}` | 取消 ASR/LLM/TTS/发送并清空当前输入 |
| `{"type":"playback_done"}` | 客户端播放完毕，回到 listening |
| `{"type":"recalibrate_mood"}` | 完整重置当前会话情绪 |
| `{"type":"ping"}` | 应用层探活 |

### 服务端下行

回复同样使用明确边界：

```text
text:   {"type":"reply_start","turn_id":"...","text":"...",
         "audio_format":"pcm_s16le","sample_rate":24000,"audio_bytes":96000}
binary: <PCM chunk 1>
binary: <PCM chunk 2>
text:   {"type":"reply_end","turn_id":"...","audio_bytes":96000}
```

`audio_bytes=0` 表示本轮只有文字。错误统一用
`{"type":"error","code":"...","message":"...","recoverable":true}`，避免把失败伪装成静音。

状态机为 `listening → transcribing → thinking → speaking → listening`。活动状态与情绪状态
分开维护，因此进入 “thinking” 不会把用户刚触发的 happy/sad 情绪覆盖掉。

## 中断语义

接收循环不会等待完整对话管线，而是把当前轮次放入独立任务。收到 `interrupt` 或新的
`audio_start`/文字输入时，服务端取消旧任务。ESP32 同时停止本地播放并忽略旧回复后续帧，
所以按下 PTT 可以立即 barge-in，而不是等旧语音播完。`whisper_local` 的 CPU 推理在线程中
运行，取消后不会再使用它的结果，但底层线程可能仍会把当前推理计算完。

## 固件音频路径

```text
INMP441 32-bit I2S1 RX
  → 右移/饱和到 PCM16
  → 约 32 ms 二进制块
  → WebSocket

WebSocket 二进制块
  → 32 KiB stream buffer
  → I2S0 TX（按 reply_start 重配时钟）
  → MAX98357A
```

固件不再申请 15 秒、约 480 KiB 的连续录音区。固定占用主要是 32 KiB 播放缓冲和任务栈。

## 容量与限制

- PCM16 单声道 16 kHz：约 32 KB/s，即 256 kbit/s；24 kHz：约 48 KB/s。
- 服务端默认最多 16 条 WebSocket，会拒绝超额连接。
- 单条 JSON 控制帧在固件端上限 4096 bytes，并支持 WebSocket 分片重组。
- 当前没有鉴权；局域网使用 `ws://`，公网必须在反向代理处加 TLS 与认证。
- `wss://` 固件启用公共 CA bundle，不再跳过证书校验。

## Provider 真实能力

- `toy-brain` 只提供演示回复，不等同于通用 LLM。
- `ASR=disabled` 时语音入口被禁用，文字仍可用。
- edge-tts 需要 `edge-tts` 包、网络和 `ffmpeg`。
- `whisper_local` 会在线程中运行，避免阻塞其他连接，但模型本身仍消耗较多 CPU/内存。
