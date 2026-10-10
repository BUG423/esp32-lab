# 🐾 bytepet — 电子宠物复活了

> **桌面数字宠物回来了。** 跑在 ¥15 的 ESP32 上，用浏览器喂养，未来还会接入 AI 开口说话。

[English](README.md) · **中文**

---

## 📖 故事

小时候藏在书包里的电子宠物，现在住在你桌面上的 ESP32 里。
养一只「小帕」，喂它、陪它玩、给它洗澡。它会从一颗蛋破壳而出，慢慢长大成人；
如果你不理它两天，它会变成幽灵（再喂三次就能复活）。

电子宠物复活了，而且它准备好迎接 AI 时代：未来接入大语言模型，小帕就能开口说话，
变成你的 AI 语音伴侣。

## ✨ 功能特性

| 功能 | 说明 |
| --- | --- |
| 🥚 生命周期 | `蛋 → 幼年 → 少年 → 成年 → 幽灵`，可复活 |
| 📊 四维属性 | 饱食 / 心情 / 精力 / 清洁（0–100） |
| 🎭 情绪系统 | 开心 · 饿了 · 累了 · 脏了 · 难过 · 生病 · 睡觉 · 幽灵 |
| 🌐 网页面板 | 暗色可爱主题、纯 CSS 动画宠物、属性条、操作按钮 |
| 🔴 WebSocket | 每秒实时推送状态到浏览器 |
| 🔘 实体按钮 | 短按 = 喂食，长按 = 玩耍 |
| 💾 掉电记忆 | NVS 持久化，重启后宠物还在 |
| ⏰ 时间成长 | `esp_timer` / FreeRTOS 任务驱动宠物成长 |

## 🧾 物料清单（BOM）

| 零件 | 说明 | 参考价 |
| --- | --- | --- |
| 任意 ESP32 开发板 | ESP32-WROOM-32 / DevKitC / C3（改 target） | ~¥15 |
| 轻触按键 | 可选，默认 GPIO0（直接用 BOOT 键） | ~¥1 |
| USB 数据线 | 供电 + 烧录 | — |

**总成本不到 ¥20** —— 比 1997 年的正版电子宠物便宜多了。

## 🚀 快速开始

```bash
# 1. 安装 ESP-IDF v5.5.x
#    https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/

# 2. 配置 WiFi（可选 —— 留空则开启热点 "bytepet-setup" / "bytepet123"）
cd firmware
. ../../esp/esp-idf/export.sh
idf.py menuconfig   # bytepet Configuration → WiFi SSID / password

# 3. 编译并烧录
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor

# 4. 打开控制台
#    http://<设备IP>/   （热点模式则是 http://192.168.4.1/）
```

### 按键接线

```
ESP32 GPIO0 (BOOT) ──── 按键 ──── GND
```

芯片内部上拉已开启，无需外接电阻。

## 🐣 宠物生命周期

```
        首次喂食
           │
   🥚 蛋 ──┴─► 🐣 幼年 ──(1 小时)──► 🐥 少年 ──(3 小时)──► 🐶 成年
                                                              │
                                                     （忽略 2 天）
                                                              ▼
                                                          👻 幽灵
                                                              │
                                                    （再喂食 3 次）
                                                              ▼
                                                            🐶 成年
```

**每分钟属性漂移：** 饱食 ↑、清洁 ↓、精力 ↓、心情向中值靠拢（差属性会拉低）。

**情绪由属性推导** 而非独立存储 —— 饱食 ≥85 判定「饿了」，精力 ≤15 判定「累了」，
心情 ≤25 判定「难过」，依此类推。

## 🆚 同类产品对比

| | Tamagotchi (1997) | Giga Pets | 开源 ESP32 宠物 | **bytepet** |
| --- | --- | --- | --- | --- |
| 硬件成本 | $20+ | $15+ | 不等 | **~¥15** |
| 网页控制台 | ✗ | ✗ | 部分 | **✓ 可爱动画** |
| 实体按钮 | ✓ | ✓ | ✓ | **✓** |
| 掉电记忆 | ✓ | ✓ | 多数 | **✓ NVS** |
| 实时推送 | ✗ | ✗ | ✗ | **✓ WebSocket 1Hz** |
| AI 就绪 | ✗ | ✗ | ✗ | **✓ 路线图** |
| 开源 | ✗ | ✗ | ✓ | **✓ MIT** |

## 🗺 路线图

- [ ] 🔊 语音 / 喇叭反馈
- [ ] 🤖 接入大模型对话（「小帕会说话了」— 对接 AI 语音伴侣项目）
- [ ] 📱 PWA 离线缓存
- [ ] 🎨 自定义皮肤 / 主题
- [ ] 🥚 多宠物
- [ ] 🌍 MQTT 远程控制
- [ ] 📈 属性历史曲线

## 📁 项目结构

```
bytepet/
├── README.md              ← English
├── README_zh.md           ← 你在这里
├── LICENSE                ← MIT
├── docs/architecture.md   ← 设计文档
└── firmware/
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── partitions.csv
    └── main/
        ├── main.c           入口
        ├── pet.c/.h         状态机、属性、进化
        ├── wifi_manager.c/.h  STA + 热点回退
        ├── web_server.c/.h  HTTP + WebSocket
        ├── button.c/.h      GPIO 短按/长按
        └── web/index.html   暗色可爱控制台
```

## 📜 协议

[MIT](LICENSE)

---

> *电子宠物复活了 —— 它就住在你的浏览器里。* 🩷
