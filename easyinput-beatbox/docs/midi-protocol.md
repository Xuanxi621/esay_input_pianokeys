# EasyInput Beatbox MIDI 语义

这是软硬件联动的**语义合同**。  
当前固件用 USB Serial 承载同等领域事件（见 `host-protocol.md`）；标准 MIDI 字节由适配层编解码。USB MIDI class device 因 ESP32-S3 共享 PHY / macOS 枚举问题暂缓，见 `usb-midi-spike.md`。

## 为什么用 MIDI 语义

- Start / Stop / Continue / Clock / SPP 是专业设备通用运输语义
- 通道 10 打击乐映射与 GM 兼容，鼓机可自然扩展
- 电脑端未来可接 WebMIDI / DAW；当前阶段用 Web Serial 承载同等字段
- 板端继续负责实时发声；电脑端负责可视化与编辑

## 时钟换算

| 单位 | 数量 |
| --- | --- |
| 内部 PPQN | 96 |
| MIDI Clock PPQN | 24 |
| 1 MIDI Clock | 4 内部 tick |
| 1 十六分音符 | 24 内部 tick = 6 MIDI Clock = 1 MIDI Beat (SPP) |
| 1 四分音符 | 96 内部 tick = 24 MIDI Clock |

## Device → Host（标准 MIDI 映射）

| 消息 | 含义 |
| --- | --- |
| `0xFA` Start | 从 song position 0 开始 |
| `0xFB` Continue | 从当前 song position 继续 |
| `0xFC` Stop | 停止；保留 song position |
| `0xF8` Clock | 运行中 24 PPQN |
| `0xF2` SPP | 14-bit MIDI Beat（十六分音符计数） |
| Ch.10 Note On | 鼓音 / click：见下表 |
| Ch.10 Note Off | 对应释音（短采样可忽略） |

### GM Channel 10 映射

| Note | 用途 | 板端按键 |
| --- | --- | --- |
| 42 | Closed HH | S1 |
| 46 | Open HH | S2 |
| 39 | Clap | S3 |
| 37 | Rimshot | S4 |
| 36 | Kick | S5 |
| 38 | Snare | S6 |
| 76 | 重拍 click（High Wood Block） | 节拍器 accent |
| 77 | 普通拍 click（Low Wood Block） | 节拍器 normal |

控制键（非音符）：

| 按键 | 功能 |
| --- | --- |
| S7 | 短按 A/B；长按 Fill |
| S8 / 编码器按压 | Play / Stop |

## Host → Device（标准 MIDI 映射）

| 消息 | 含义 |
| --- | --- |
| Start / Continue / Stop | 运输 |
| Clock | 仅在未来 external-clock slave 模式使用 |
| SPP | 定位到十六分音符边界 |
| Ch.10 Note On | Live Pad / 外部触发 |

## 非 MIDI 的设备管理命令

以下保留在 Serial JSON，**不要**伪装成通用 MIDI CC：

- BPM（60–240）
- Swing（50–75）
- Pattern get/set + revision
- Click enable
- Save

历史草案中的 CC16/CC17 BPM 编码已废弃，避免与厂商自定义 CC 冲突并误导 DAW 用户。

## 适配层

TypeScript 纯函数见 `app/src/midi-adapter.ts`：

- 领域事件 → MIDI 字节
- MIDI 字节 → 领域事件
- 不依赖 Web Serial / WebMIDI 传输实现
