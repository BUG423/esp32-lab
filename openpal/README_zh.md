# 🤝 OpenPal

**开源 · 本地优先的 AI 语音搭子 —— 可自托管的「小智」替代方案。**

小智火了，但绑定官方云、链路长。OpenPal 把同样体验拆开做透明，并加上三件小事智没有的事：
**完全本地可跑 · 情绪引擎 · 浏览器即设备**。

```
 🎙️ 你说话  ──►  OpenPal Server  ──►  🔊 有情绪的语音回复
                  ASR → LLM → TTS
                  + 人格 / 情绪引擎
```

[English](./README.md) · [架构](./docs/architecture.md) · [与小智对比](./docs/compare-xiaozhi.md)

---

## ✨ 和小智的差异

| | 小智 xiaozhi-esp32 | **OpenPal** |
|---|---|---|
| 服务端 | 官方云为主 | **完全自托管**，本地 LLM/TTS 可选 |
| 上手 | ESP-IDF 6.x + 复杂配置 | ESP-IDF 5.5 + 浏览器 5 分钟体验 |
| 情绪 | 表情贴图 | **连续情绪模型**（valence/arousal）+ LED + 语气 |
| 无硬件 | 需要板子 | **浏览器麦克风直接玩** |
| LLM | 主要国内大模型 | 任意 OpenAI 兼容 API / Ollama |
| 隐私 | 依赖云端 | **语音可不出局域网** |

---

## 🚀 5 分钟跑起来（不用买硬件）

```bash
cd server
python3 -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt

# 可选：配 LLM（不配也能跑 toy-brain demo）
export PAL_LLM_API_KEY=sk-...
export PAL_LLM_BASE_URL=https://api.deepseek.com/v1

python -m pal_server.main
# 打开 http://localhost:8080
```

- 打字聊天，或点 **🎙️ 按住说话** —— 浏览器麦克风完整语音闭环
- 右侧实时看**情绪引擎**（愉悦度 / 唤醒度 / LED 色）

---

## 🧰 硬件（可选，真机）

| 部件 | 型号 | 价格 |
|---|---|---|
| 主控 | ESP32 / ESP32-S3 DevKit | ¥15–30 |
| 麦克风 | INMP441 | ¥8 |
| 功放喇叭 | MAX98357A + 喇叭 | ¥10 |
| 按键 | PTT 轻触开关 | ¥1 |

总 BOM **< ¥50**。

```bash
cd firmware
idf.py set-target esp32
idf.py menuconfig      # WiFi + 服务器 ws://<PC-IP>:8080/ws
idf.py build flash monitor
# 按住 PTT 说话 → 松开 → 语音回复
```

---

## 🧠 情绪引擎（独有）

```
用户话语 ──► 情绪打分 ──► EMA 平滑 ──► (valence, arousal)
                                │
                ┌───────────────┼───────────────┐
                ▼               ▼               ▼
           系统提示词       LED 颜色         TTS 语速
```

开心 → 语速快、灯绿；难过 → 温柔、灯蓝紫；兴奋 → 能量感、灯黄。
点控制台**光球**随时重置。

---

## 📁 目录

```
openpal/
├── server/                 Python 服务端
│   ├── pal_server/         main / personality / asr / llm / tts
│   └── static/index.html   控制台 + 浏览器语音
├── firmware/               ESP-IDF 固件（I2S + WS + PTT）
└── docs/                   架构 + 与小智对比
```

---

## 🔌 接入任意 LLM

```bash
# DeepSeek
export PAL_LLM_BASE_URL=https://api.deepseek.com/v1
export PAL_LLM_API_KEY=sk-xxx

# 本地 Ollama
export PAL_LLM_BASE_URL=http://127.0.0.1:11434/v1
export PAL_LLM_API_KEY=ollama
export PAL_LLM_MODEL=qwen2.5:7b
```

---

## 🛠️ 内置工具（MCP 风格函数调用）

无 API Key 也能触发（toy-brain 关键词路由）；接上真 LLM 后支持完整 function calling：

| 工具 | 触发示例 | 说明 |
|---|---|---|
| `get_time` | 「现在几点？」 | 日期 / 时间 / 星期 |
| `get_weather` | 「北京天气？」 | wttr.in 免费，无需 Key |
| `get_joke` | 「讲个笑话」 | 中文笑话库 |
| `control_led` | 「开灯」 | 广播到设备 / 浏览器 |
| `remember` / `recall` | 「记住买牛奶」 | 短期笔记 |
| `compute` | 「12*13」 | 安全数学表达式 |

控制台会实时显示每一次工具调用。加新工具只要在 `tools.py` 加 20 行 —— 见 [CONTRIBUTING](./CONTRIBUTING.md)。

---

## 🗺️ 路线图

- [x] 浏览器语音闭环
- [x] 情绪引擎
- [x] ESP32 PTT 对讲
- [x] 本地/云端 LLM 切换
- [x] **MCP 风格工具调用**
- [x] 测试 + GitHub CI
- [ ] 唤醒词（ESP-SR / OpenWakeWord）→ [docs/wake-word.md](./docs/wake-word.md)
- [ ] Opus 编解码
- [ ] 屏幕表情 / OTA / Home Assistant

---

## 和小智的关系

**不是** fork，是同一赛道的另一种取舍：
- 小智：产品化程度高、生态大、开箱即用
- OpenPal：可拆解、可自托管、可改造

想直接玩成品 → 小智；想自己长出一个属于自己的 AI 搭子 → OpenPal。

## License

MIT
