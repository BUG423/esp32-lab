# 🎵 bassline

**ESP32 音乐频谱可视化器** —— 麦克风采样 + FFT，驱动 WS2812 灯带，并把频谱实时推送到霓虹风网页仪表盘。

[English → README.md](README.md)

---

## ✨ 功能特性

- 🎤 **声音采集** —— I2S 数字麦（INMP441 / ICS-43434）**或** ADC 模拟麦，menuconfig 一键切换
- 📈 **实数 FFT** —— 纯 C 实现的基-2 迭代 FFT（256 / 512 点）+ Hann 窗，零第三方依赖
- 🌈 **32–64 个对数频段** —— 贴近人耳的频率映射（低音 → 高音）
- 💡 **WS2812 / NeoPixel 灯带** —— 频谱柱、VU 表、呼吸、闪烁四种模式
- 🎨 **配色主题** —— 彩虹（蓝→青→绿→黄→红）、烈火、寒冰
- 📉 **峰值保持 + 平滑衰减** —— 摄像头里特别上镜
- 🌐 **网页仪表盘** —— 暗色霓虹 UI（`#0b1020`）、实时 canvas 频谱、VU 表、灯带镜像、完整控制面板
- 🔌 **WebSocket 直播** —— 约 30 fps 频谱帧 + JSON 控制通道
- 📱 **热点 + 路由器双模** —— 开箱即用 `bassline / bassline123`，配置后自动连家里 Wi-Fi

## 🎬 适用场景

| 场景 | 氛围 |
|---|---|
| 🖥️ 桌面音乐可视化 | 霓虹光柱随歌单起舞 |
| 🎤 演唱会应援 | 跟着鼓点挥舞的手持灯 |
| 🕹️ 电竞房灯效 | 直播/打游戏的声光电氛围 |
| 📹 30 秒短视频素材 | 高能 B-roll 随手拍 |

## 🛒 物料清单（约 ¥25）

| 器件 | 参考价 | 备注 |
|---|---|---|
| ESP32 DevKit（WROOM-32） | ~¥12 | 任意经典 ESP32 开发板 |
| INMP441 I2S 麦克风模块 | ~¥8 | 或 ICS-43434 / MSM261 |
| WS2812 灯带 60 灯 | ~¥15 | 5V，60 灯/米密度刚好 |
| USB 线 + 5V 电源 | — | 灯带建议 ≥1 A |

## 🔌 接线

### INMP441（I2S）

| INMP441 | ESP32 |
|---|---|
| VDD | 3V3 |
| GND | GND |
| SD（数据） | GPIO 33（`BASSLINE_I2S_DIN_GPIO`） |
| SCK（BCLK） | GPIO 26（`BASSLINE_I2S_BCLK_GPIO`） |
| WS | GPIO 25（`BASSLINE_I2S_WS_GPIO`） |
| L/R | 接 GND（左声道） |

### WS2812 灯带

| WS2812 | ESP32 |
|---|---|
| DIN | GPIO 5（`BASSLINE_LED_GPIO`） |
| 5V | 5V 电源（≥1 A） |
| GND | 与 ESP32 共地 |

> 💡 DIN 串 300–500 Ω 电阻，5V/GND 并 1000 µF 电容，波形更干净。

### ADC 模拟麦（可选）

输出接到 ADC1 引脚（GPIO 32–39），menuconfig 里设 `BASSLINE_MIC_ADC=y` 与 `BASSLINE_ADC_GPIO`。

## 🚀 快速开始

```bash
cd firmware
. ../../esp/esp-idf/export.sh
idf.py set-target esp32
idf.py menuconfig          # 可选：WiFi、引脚、灯珠数量
idf.py build flash monitor
```

手机 / 电脑连接热点 **`bassline`**（密码 `bassline123`），浏览器打开 **http://192.168.4.1/**

若在 menuconfig 填了家庭 WiFi，板子同时会拿到局域网 IP，用那个 IP 访问即可。

## 🧠 工作原理（FFT）

1. **采样** 256（或 512）点 @ 16 kHz（约 16 ms 窗口）。
2. **Hann 窗** 抑制频谱泄漏。
3. **基-2 FFT** 得到幅度谱（`n/2` 个线性频点，256 点 @16k 时每点 31.25 Hz）。
4. **对数折叠** 合并为 48 个对数频段（55 Hz – 8 kHz），符合人耳音高感知。
5. **归一化**（增益 + 对数压缩），**峰值保持** + 指数衰减。
6. **渲染** 到灯带（按强度渐变上色），并以 30 fps 通过 WebSocket 广播 JSON。

```
mic ──► window(256) ──► Hann ──► FFT ──► |mag| ──► log-bins(48) ──┬──► WS2812
                                                                     └──► WS /dashboard
```

## 🆚 与 WLED 的对比

| | **WLED** | **bassline** |
|---|---|---|
| 定位 | 海量 LED *特效* | **音频响应式频谱** |
| 音频 | 外部 sound-sync，能力有限 | **片上 FFT + 麦克风** |
| 仪表盘 | 功能庞大的通用 UI | **实时频谱 + VU** |
| 代码量 | 大而全 | **小巧可改，IDF 原生** |

想看漂亮灯效用 **WLED**；想让灯**跟着音乐跳舞**，用 **bassline**。

## 🗺️ 路线图

- [ ] BLE MIDI / 按键切换模式
- [ ] 麦克风 AGC + 噪声门
- [ ] 更多主题（日落、矩阵、马卡龙）
- [ ] OLED / LED 点阵输出
- [ ] OTA 固件升级
- [ ] 立体声双麦空间效果

## 📄 许可证

MIT —— 详见 [LICENSE](LICENSE)。
