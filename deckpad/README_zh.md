# 🎛️ DeckPad

**¥20 做一个 BLE 宏键盘 / 直播控制台。** 按一下，快捷键就发出去。

手机、平板、电脑——只要支持 BLE 键盘就能用。Web 页面随时改宏，NVS 掉电不丢。

[English](./README.md) · [架构设计](./docs/architecture.md)

---

## ✨ 特性

| | |
|---|---|
| 🎹 **BLE HID 键盘** | 标准键盘协议，Win/Mac/Android/iOS 通用 |
| ⌨️ **4 个宏按键** | 物理按键 + 网页点击都能触发 |
| 🧅 **3 层宏** | 按键 0 长按切换 L0 流程 / L1 音量 / L2 切歌 |
| 🎵 **多媒体键** | 音量、静音、播放/暂停、上下曲（HID Consumer） |
| 🌐 **Web 配置页** | SoftAP 直连 `192.168.4.1`，改宏即时生效 |
| 💾 **NVS 持久化** | 断电宏与当前层都还在 |
| 📦 **极简 BOM** | 1 块 ESP32 + 4 个按键 |

---

## 🧰 硬件

| 部件 | 说明 | 价格 |
|---|---|---|
| ESP32 DevKit | 任意 | ¥15 |
| 轻触开关 ×4 | GPIO 0 / 4 / 16 / 17（可 menuconfig 改） | ¥1 |

接线：按键一端接 GPIO，一端接 GND（内部上拉）。

---

## 🚀 快速开始

```bash
cd firmware
idf.py set-target esp32
idf.py menuconfig     # 改按键 GPIO / BLE 名称
idf.py build flash monitor
```

1. 手机/电脑 **蓝牙设置** → 找到 `DeckPad` → 配对
2. 手机连接 WiFi 热点 `DeckPad-XXXX`（无密码）
3. 打开 `http://192.168.4.1` 编辑宏
4. 按按键或点网页按钮 → 快捷键触发

---

## 🎯 预置宏

| 槽位 | 名称 | 默认动作 |
|---|---|---|
| 0 | Mute Mic | Ctrl+Shift+M |
| 1 | Action 2 | Ctrl+Shift+Z |
| 2 | Hello Text | 发送文本 |
| 3 | Copy | Ctrl+C |

**直播场景：** 槽位 0 静音麦、槽位 1 切场景、槽位 2 发弹幕固定语、槽位 3 复制链接。

---

## 🔑 HID 键码速查

| 键 | 码 |
|---|---|
| a-z | 4-29 |
| 1-9 | 30-38 |
| 0 | 39 |
| Enter | 40 |
| Space | 44 |
| 修饰键 | 1=Ctrl · 2=Shift · 4=Alt |

---

## 📁 目录

```
deckpad/
└── firmware/main/
    ├── bt_hid.c      BLE HID 键盘协议
    ├── macros.c      宏存储 + NVS
    ├── buttons.c     物理按键
    ├── web_server.c  配置页 API
    └── web/index.html
```

---

## 🗺️ 路线图

- [x] BLE HID 键盘
- [x] Web 宏编辑 + NVS
- [ ] OLED 显示当前宏
- [ ] 旋钮（音量）
- [x] 多层（长按切换宏组）
- [x] 多媒体键（音量/播放）
- [ ] 3D 打印外壳

---

## License

MIT