# GlowLink 架构

## 物理层

```
        TX 模式                          RX 模式
   GPIO ──[220Ω]── LED ── GND      ADC ── LED ── GND
   高电平点亮，低电平灭             LED 反偏/光伏，光生电压被 ADC 采样
```

### 曼彻斯特编码

```
bit 1:  ──┐     ┌──
          └─────┘     (先高后低)

bit 0:  ──────┐
              └──     (先低后高)
```

每个 bit = 2 个半周期。`baud=250` → 半周期 = 2000 µs。

## 帧协议

```
偏移  长度  内容
0     3     PREAMBLE  0xAA 0xAA 0xAA
3     1     LEN       payload 长度 (1..200)
4     LEN   PAYLOAD   UTF-8 文本或二进制
4+LEN 1     CRC8      XOR(payload)
```

接收端状态机：
```
滑窗匹配 preamble → 读 LEN → 读 PAYLOAD → 校验 CRC
   ↓ 成功                  ↓ 失败
 回调应用层            rx_errors++，丢弃
```

## 自适应阈值

RX 比特判决：

```
half1 = avg(ADC during first half)
half2 = avg(ADC during second half)
bit = (half1 > half2) ? 1 : 0
```

噪声基线用慢 EMA 跟踪，`optical_calibrate()` 在启动和手动校准时采 200ms 环境光。

## Web 可视化

```
optical_get_stats() ──► /ws 每 100ms 广播 JSON
                          {
                            "signal": 812, "snr_db": 18.4,
                            "quality": 72, "wave": [128 samples],
                            "tx": 12, "rx": 10, "err": 1,
                            "log": ["TX hello", "RX hello"]
                          }
```

浏览器 Canvas 画荧光波形 + 消息控制台。

## 双机角色

| 菜单配置 | 行为 |
|---|---|
| `GLOWLINK_IS_SENDER=y` | 每 5s 自动 beacon 一条消息 |
| `GLOWLINK_IS_SENDER=n` | 纯接收 |

Web 面板可随时手动发送，不受角色限制（半双工，同机收发要靠协议层调度）。

## 光配对与加密

配对用控制帧（payload 首字节 = 类型）：

| 类型 | 含义 | 载荷 |
|---|---|---|
| `0xB0` | PAIR_OFFER | 8 字节随机密钥 |
| `0xB1` | PAIR_ACK | 8 字节密钥回显 |
| `0xB2` | ENC_MSG | 密钥流 XOR 密文 + 1 字节校验和 |

流程：

```
节点 A（发起）                      节点 B（接受）
pairing_start()
  生成 8 字节随机密钥
  发送 PAIR_OFFER ─────────────►  pairing_on_payload()
                                    存密钥 → NVS
                                    发送 PAIR_ACK ──► 校验密钥 → NVS
  状态 = PAIR_PAIRED
```

之后 `protocol_send_text()` 自动走 `pairing_protect()` 加密，接收端 `pairing_on_payload()` 解密后才交给文本回调。未配对时退化为明文，与旧版兼容。

密钥只经过光信道（不经过 WiFi / 云），这是「光配对」的叙事核心。加密是**演示级**密钥流 XOR（8 字节重复密钥 + 校验和），不是工业级密码学。

## 与 SecurePair 的差异

| | SecurePair | GlowLink |
|---|---|---|
| 演示重点 | 密钥交换 + ESP-NOW 混合 | **可视化示波器** + 低门槛复刻 |
| 可视化 | 视频 | Web 面板实时波形/SNR |
| 文档 | 英文 | **中英双语** |
| 协议 | 自有 | 曼彻斯特 + 透明帧 + 光配对控制帧 |
