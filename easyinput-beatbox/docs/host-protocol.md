# EasyInput Beatbox 主机协议

## 结论

对这套板子，**当前主链路是 USB-Serial/JTAG + Web Serial**。  
消息是**领域命令/事件**；它们可以映射到 MIDI 语义（见 `midi-protocol.md`），但 JSON 本身不是 MIDI 字节流。

为什么不先上 USB MIDI class device：

- ESP32-S3 的 USB-OTG 与 USB-Serial/JTAG **共用一根 PHY**
- macOS 上 ROM 会先枚举成 JTAG/Serial；TinyUSB 再抢口时经常失败或需拔插/烧 eFuse
- EasyInput 日常烧录也依赖该串口；先保证配套 UI 稳定自动连接更重要

## 时钟合同

| 域 | 分辨率 | 说明 |
| --- | --- | --- |
| 板端内部 | **96 PPQN** | 音频采样时钟派生；唯一实时主时钟 |
| 对外 MIDI Clock | **24 PPQN** | 1 MIDI Clock = 4 内部 tick |
| 16 分音符 | 24 内部 tick | 1 MIDI Beat (SPP) = 6 MIDI Clock = 24 tick |
| 四分音符 | 96 内部 tick | 节拍器 accent 周期 |

板端永远是音频主时钟。主机不得驱动发声时基。

## 帧格式

UTF-8，一行一条 JSON，`\n` 结尾。非 `{` 开头的行忽略（兼容偶发日志）。

协议版本：`hello.v = 2`。v1 客户端仍可读 `state` / `beat` / `start` / `stop`。

### Device → Host

| 消息 | 含义 |
| --- | --- |
| `{"t":"hello","v":2,"name":"EasyInput Beatbox","caps":["drum","pattern","swing","volume","record"]}` | 身份与能力 |
| `{"t":"state","bpm":120,"run":0,"beat":0,"step":0,"bar":0,"tick":0,"swing":50,"var":0,"fill":0,"rev":1,"click":1,"mode":0,"vol":100}` | 完整状态快照（约 2 Hz + 事件） |
| `{"t":"position","bar":0,"step":0,"beat":0,"tick":0,"accent":1}` | 步进位置（播放中） |
| `{"t":"beat","accent":1,"beat":0,"step":0}` | 四分拍点（v1 兼容；含 step0–15） |
| `{"t":"note","n":36,"v":127}` | 实时打击（Live Pad / 序列触发回显） |
| `{"t":"key","i":0,"v":1}` | 物理键反馈：`i`=0..7 对应 S1..S8，`v`=按下/抬起 |
| `{"t":"pattern","bank":0,"rev":1,"p":"<192 hex>"}` | 单 bank Pattern（96 字节 velocity 的 hex）；全量同步时连发 bank 0/1/2 三行，避免长行截断 |
| `{"t":"start"}` / `{"t":"continue"}` / `{"t":"stop"}` | 运输变化 |
| `{"t":"ack","cmd":"pattern_set","ok":1,"rev":2}` | 命令确认 |
| `{"t":"error","cmd":"...","msg":"..."}` | 命令失败 |

字段约定：

- `beat`：小节内四分拍 `0..3`
- `step`：16 步网格位置 `0..15`
- `tick`：小节内 96 PPQN 位置 `0..383`（4 拍 × 96）
- `swing`：`50..75`（百分数；50 = 直拍）
- `var`：`0=A` / `1=B`
- `fill`：`0/1` Fill 按住态
- `rev`：Pattern 修订号，仅用于显示与同步排序；设备采用最后写入者生效，不向用户暴露冲突
- `mode`：鼓机层开关，`0` 关 / `1` 开；上电默认 `0`（可与节拍器同时开）
- `vol`：总音量 `0..127`（默认 `100`）
- `click`：节拍器 click 层开关，与鼓机层独立；上电默认 `1`
- `p` / `a` / `b` / `f`：`6 × 16 = 96` 字节，按轨串联（Kick / Snare / CHH / OHH / Clap / Rim），每字节 velocity `0`=关、`1..127`=开

### Host → Device

| 消息 | 含义 |
| --- | --- |
| `{"t":"start"}` | 从 step 0 / tick 0 开始 |
| `{"t":"continue"}` | 从停止位置继续 |
| `{"t":"stop"}` | 停止并保留位置 |
| `{"t":"bpm","v":128}` | 设置 BPM（60–240） |
| `{"t":"swing","v":66}` | 设置 Swing（50–75） |
| `{"t":"variation","v":0}` | 选择 A/B（量化到下一 16 分边界生效） |
| `{"t":"fill","v":1}` | 进入/退出 Fill 覆盖 |
| `{"t":"note","n":36,"v":127}` | Live Pad 触发 |
| `{"t":"click","v":1}` | 启用/关闭节拍器 click 层 |
| `{"t":"mode","v":0}` | 启用/关闭鼓机 Pattern 层 |
| `{"t":"volume","v":100}` | 设置总音量（0–127） |
| `{"t":"pattern_get"}` | 请求 `pattern_dump` |
| `{"t":"pattern_set","bank":0,"rev":1,"p":"<192 hex>"}` | 写入单 bank；设备采用最后写入者生效 |
| `{"t":"save"}` | 请求将 Pattern 标记为已保存（MVP：bump rev + ack；NVS 后续） |
| `{"t":"record","v":1}` | 主机叠录武装：`1` 时锁定 S7 / S8 / 编码器启停；`0` 解除 |
| `{"t":"ping"}` | 请求 `hello` + 紧随 `state` + `pattern_dump` |

## 运输语义

| 命令 | 行为 |
| --- | --- |
| Start | 位置归零，下一音频块起跑 |
| Stop | 停止发声调度，保留 `bar/step/tick` |
| Continue | 从保留位置继续（不归零） |

## 配套 UI

`app/` 使用 Web Serial：

1. 首次点击「连接」，授权 Espressif（VID `0x303A`）串口
2. 之后插拔可自动重连（`navigator.serial.getPorts()`）
3. 仅在收到合法 `hello` + `state` 后进入 `synced`
4. Pattern 编辑使用本地 draft + `rev` 提交；冲突时以设备权威并提示用户

## 与 MIDI 的对应

见 `midi-protocol.md`。摘要：

| 领域事件 | 标准 MIDI |
| --- | --- |
| start / continue / stop | `0xFA` / `0xFB` / `0xFC` |
| 内部 tick ÷ 4 | `0xF8` Clock（24 PPQN） |
| step 位置 | Song Position Pointer（1 MIDI Beat = 1/16 音符） |
| note n/v | Ch.10 Note On/Off |
| bpm / pattern / swing / rev | **设备管理命令**（Serial JSON）；不是通用 MIDI CC |

不要再声称 CC16/17 是标准 BPM。
