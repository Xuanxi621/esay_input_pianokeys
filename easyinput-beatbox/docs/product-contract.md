# EasyInput Beatbox 产品合同

硬件事实以 `easyinput-board-cy` 为准；本文件描述产品层行为。

## 音色目标

参考录音室 click / clave / 木块实践：

- 干声、短瞬态、锐利起音（当前 click 约 9 ms）
- 主体约 2.0–2.5 kHz clave 感，而不是长鸣正弦或刺耳方波
- 削去过高频能量，降低长时间听感疲劳
- 重拍与普通拍以音高/音色区分，音量差保持克制
- 鼓机默认音色：public-domain TR-707 Kick/Snare/CHH/OHH/Clap，离线转换为 32 kHz/16-bit/mono PCM 并嵌入固件

板端合成实现见 `firmware/main/audio/audio_click.c`。

## 时钟与音频链路

- I2S `32000 Hz` 采样时钟是唯一节拍时间源；UI、USB 与灯光不得驱动声音触发
- 连续渲染 `128 frame`（4 ms）音频块，不在每拍临时启动/停止 DMA
- 内部调度分辨率 **96 PPQN**（由采样游标 Q32 相位累计派生）
- 对外 MIDI Clock 为 **24 PPQN**（每 4 个内部 tick 导出一发）
- 16 步网格：每步 = 24 tick；Swing 仅延迟奇数 16 分音符
- 外部音符进入队列；预分配 voice 独立播放并在 `int32` 累加器中混音
- 音频线程不分配内存、不等待 UI/USB 锁，也不执行灯带更新
- `scripts/check_timing_model.py` 必须覆盖 60–240 BPM 与 Swing；允许误差不超过 1 sample

## 交互目标

### 节拍器

- 板端可独立使用：旋钮 BPM + 编码器/S8 启停
- 上电默认为「仅节拍器」模式：不运行鼓序列，只播放 click
- click 开关关闭后节拍器模式静音；鼓机模式下该开关只控制是否叠加 click
- 视觉可预期下一拍（灯来回走；电脑端拍点条）
- 电脑端连接后自动识别设备，显示 BPM、运行态、拍号
- 键盘：空格启停，方向键微调 BPM

### 鼓机（P2）

| 丝印 | 功能 |
| --- | --- |
| S1 | Closed HH（GM 42） |
| S2 | Open HH（GM 46） |
| S3 | Clap（GM 39） |
| S4 | Rimshot（GM 37） |
| S5 | Kick（GM 36） |
| S6 | Snare（GM 38） |
| S7 | 短按 A/B；长按 Fill |
| S8 / 编码器按压 | Play / Stop |

4×2 排布遵循常见 finger-drumming / MPC 习惯：底排放 Kick·Snare 根基，上排放镲片与打击乐。

- 默认 4/4、16 步、6 轨（Kick / Snare / CHH / OHH / Clap / Rim）
- Swing 50–75%（主机滑块；编码器按住旋转后续可接）
- Pattern revision 双向同步；冲突时设备权威
- 主机右栏为 16 步编辑器 + Live Pad，不是 8 个鼓音色假 Pad
- **网页 REC 叠录**：只在配套 UI；写入当前编辑的 A 或 B，边播边录、不先清空；录音中锁定板端 S7/S8/编码器启停，避免误触
- Pattern JSON 导入/导出（含 A/B/Fill hex）

## 连接

- **当前主通道**：USB Serial + Web Serial（`host-protocol.md`）
- **语义合同**：MIDI 运输 / GM Ch.10（`midi-protocol.md`）
- **适配层**：领域事件 ↔ MIDI 字节（`app/src/midi-adapter.ts`）
- 选择原因：ESP32-S3 共享 PHY，macOS 上 TinyUSB MIDI 易被 JTAG/Serial 抢枚举；Serial 可稳定烧录+联动
- USB MIDI class 可行性见 `usb-midi-spike.md`；未过门槛前不替换 Serial

## 阶段

| 阶段 | 状态目标 |
| --- | --- |
| P1 | 独立节拍器 + 主机链路 + 配套 UI |
| P2 | 八键鼓机 / 16 步 / Swing / A/B / Fill，复用通道 10 语义 |
| P3 | UI 深度编辑 / Kit；可选 USB MIDI class 或桌面桥接 |
