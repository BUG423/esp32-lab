# 唤醒词 / 语音活动检测

OpenPal v1 采用 **PTT（按住说话）** + 服务端能量 VAD，保证最稳的第一体验。
唤醒词是路线图上的下一步，本文说明取舍与实现路径。

---

## 当前：PTT + 能量 VAD

### 设备端（ESP32）
- 按住按钮 → 开始采集 I2S PCM
- 松开 → 发送 `audio_end`，服务端识别

**优点：** 零误唤醒、零模型依赖、延迟最低  
**缺点：** 需要动手按

### 服务端（浏览器 / 设备流）
可选能量门限 VAD，用于自动切分长录音：

```python
# config
PAL_VAD_ENERGY_THRESHOLD=0.012
PAL_SILENCE_MS=700
PAL_MAX_UTTERANCE_MS=15000
```

逻辑：
```
PCM 帧 RMS > threshold  →  进入 speech
连续 silence_ms 静音    →  判定说话结束
超过 max_utterance_ms   →  强制切段
```

---

## 下一步：唤醒词（Wake Word）

三个可选方案：

### A. ESP-SR（乐鑫官方，设备端）
- 芯片：ESP32-S3 / ESP32-C3（带 DSP 指令的型号体验最好）
- 模型：`wn9_hilexin` 等，或用 [ESP-SR 唤醒词训练](https://github.com/espressif/esp-sr) 自训
- 优点：离线、低功耗、延迟小
- 缺点：需要 S3 级算力，模型体积几百 KB

```c
// sketch
esp_sr_init();
detect_ase_t ase = ase_create(&ase_config);
// ase_detect() -> wake event -> 开始推流
```

### B. OpenWakeWord（服务端 / 树莓派）
- Python，`pip install openwakeword`
- 可自训唤醒词（「小帕小帕」）
- 适合「网关 + 多个哑终端」架构

### C. 浏览器 WebRTC VAD + 关键词
- `webrtcvad` 做断句
- 关键词匹配做简单触发（免费、可演示）

---

## 推荐路线

```
v1  PTT                    ← 已实现，最稳
v2  服务端能量 VAD 自动断句   ← 已实现基础版
v3  浏览器唤醒词（demo 用）
v4  ESP-SR 设备端唤醒词      ← 真机体验最优
```

---

## 为什么 v1 不做唤醒词

| | PTT | 唤醒词 |
|---|---|---|
| 误唤醒 | 无 | 需调参 |
| 模型 | 无 | 需训练/下载 |
| 芯片要求 | 任意 ESP32 | S3/C3 更好 |
| 演示稳定性 | 极高 | 现场易翻车 |

**先用 PTT 把体验闭环做扎实，再上唤醒词。** 这是小智也走过的路径
（先推流对讲，后加 ESP-SR 唤醒）。

---

## 自训唤醒词（OpenWakeWord 示例）

```bash
pip install openwakeword
# 录 200 条正样本 + 负样本，按官方脚本训练
# 导出 onnx → 放进 server/models/wake.onnx
```

服务端伪代码：

```python
from openwakeword.model import Model
m = Model(wakeword_models=["models/wake.onnx"])
for frame in pcm_frames:
    score = m.predict(frame)
    if score["wake"] > 0.5:
        start_utterance()
```
