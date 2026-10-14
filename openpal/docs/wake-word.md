# 唤醒词与 VAD 状态

当前版本只实现 PTT，不包含唤醒词，也不做静音自动断句。

服务端已有的 `PAL_VAD_ENERGY_THRESHOLD` 是整段录音的最低 RMS 门槛，用来拒绝空白、
接线错误或极低音量；`PAL_MIN_UTTERANCE_MS` 和 `PAL_MAX_UTTERANCE_MS` 分别限制最短、
最长录音。这些检查不等同于帧级 VAD。

## 当前交互

```text
按下 PTT → audio_start → 连续 PCM 块 → 松开/超时 → audio_end → ASR
```

这条路径的优点是行为确定、无需常开模型；缺点是不能免手操作。

## 后续实现顺序

1. 设备端或服务端帧级 VAD，增加前滚缓冲，避免吞掉句首。
2. VAD 自动断句与超时策略，仍保留 PTT 作为可靠后备。
3. ESP32-S3 上评估 ESP-SR 离线唤醒；经典 ESP32 保持 PTT。
4. 服务端评估 openWakeWord，适合树莓派/NAS + 多个轻终端。

唤醒方案落地前，需要同时验证误唤醒率、漏检率、噪声环境、模型许可、RAM/Flash 占用，
以及唤醒后打断正在播放语音的行为。仅接入一个模型 API 并不能算完成。
