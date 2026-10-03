# 从一块开发板到一台鼓机

## EasyInput Beatbox 完整制作指南

EasyInput Beatbox 是一个基于 EasyInput V2.0 开发板的节拍器与鼓机项目。设备可以独立完成节拍调度、声音播放和按键演奏，也可以通过 USB 连接电脑，在网页中编辑 Pattern、控制播放并进行叠录。

本指南从硬件识别开始，依次完成安全上电、节拍器、音频时钟、鼓垫、十六步音序器、网页控制和 MIDI 语义映射。各章节与仓库中的固件、网页应用和协议文档相互对应，可以按照顺序逐步实现。

完成整个项目后，系统具备以下功能：

- 60–240 BPM 独立节拍器；
- 编码器调速和按键启停；
- 五颗 RGB LED 拍点提示；
- 六种鼓组音色和八键现场演奏；
- 16 步 Pattern、A/B Variation、Fill 和 Swing；
- 网页端 Pattern 编辑与实时叠录；
- USB Serial 双向同步；
- MIDI Clock、运输控制和 GM 通道 10 的语义映射。

---

## 1. 开发板与项目结构

### 1.1 开发板名称

项目资料中会出现三个名称，它们指向同一套硬件：

| 使用位置 | 名称 |
| --- | --- |
| 产品与课程 | EasyInput V2.0 |
| 固件板型 | `v2` |
| PCB 丝印 | AI Keyboard V2.1 |

结构件版本 V25、V26 只表示外壳或结构版本，不代表新的电气设计。

### 1.2 主要硬件资源

| 部件 | 硬件连接 | 本项目用途 |
| --- | --- | --- |
| 主控 | ESP32-S3R8 | 运行固件、音频与 USB 通信 |
| 存储 | 8 MB PSRAM、16 MB Flash | 固件、采样和运行缓冲 |
| USB-C | 原生 USB，GPIO19/20 | 烧录和主机通信 |
| S1–S8 | 八路独立低有效输入 | 鼓垫和功能键 |
| 旋转编码器 | A/B：GPIO17/16；按压：GPIO18 | BPM 调节和播放控制 |
| RGB LED | 5 颗 WS2812 串联，数据脚 GPIO12 | 拍点和状态显示 |
| 扬声器 | MAX98357A，GPIO14/13/15 | I2S 音频输出 |
| 麦克风 | GPIO9/10/11 | 当前版本暂未进入实时音频链路 |
| 外设电源 | GPIO8，高电平有效 | 控制 LED、麦克风和扬声器电源域 |
| BOOT | GPIO0 相关的一键下载电路 | 进入固件下载模式 |

S1–S8 的完整 GPIO 映射如下：

| 按键 | GPIO | 按下电平 |
| --- | ---: | ---: |
| S1 | 2 | 0 |
| S2 | 47 | 0 |
| S3 | 38 | 0 |
| S4 | 41 | 0 |
| S5 | 1 | 0 |
| S6 | 6 | 0 |
| S7 | 7 | 0 |
| S8 | 48 | 0 |

GPIO0 只用于 BOOT 下载入口，S5 对应 GPIO1。

### 1.3 硬件连接与产品功能

开发板资料负责描述器件、GPIO、信号方向和电源关系。Beatbox 项目在此基础上定义按键动作、音色、协议和交互。例如：

- `S5 = GPIO1，低电平表示按下` 属于硬件连接；
- `S5 触发 Kick` 属于 Beatbox 的产品设计；
- `GPIO8 控制共享外设电源` 属于硬件连接；
- `演奏期间保持共享电源开启` 属于本项目的电源策略。

仓库中的两份 Skill 分别保存这两类信息：

- `.cursor/skills/easyinput-board-cy/`：开发板资料；
- `.cursor/skills/easyinput-drum-machine/`：Beatbox 产品与固件约定。

修改引脚、电源或外设代码前，应先核对开发板资料。修改键位、节拍、Pattern 或协议时，以 Beatbox 产品约定为准。

### 1.4 仓库目录

```text
easyinput-beatbox/
├── firmware/          ESP-IDF 固件
├── app/               Vite + Web Serial 网页应用
├── desktop/           Electron 桌面封装
├── assets/samples/    鼓组采样资源
├── docs/              产品和协议文档
├── scripts/           检查与辅助脚本
└── .cursor/skills/    开发板与产品约定
```

固件中的主要模块：

```text
firmware/main/
├── board/             引脚、按键和共享电源
├── audio/             I2S、click、采样播放和混音
├── seq/               时钟、Pattern 和音序器
├── ui/                RGB 状态显示
├── host/              USB Serial 主机协议
└── main.c             模块初始化与事件协调
```

---

## 2. 系统架构与开发阶段

### 2.1 板端与电脑端的职责

节拍和声音对实时性要求很高，因此由开发板独立完成。电脑端处理图形界面、Pattern 编辑、状态显示和数据同步。

```text
按键 / 编码器 / 主机命令
            │
            ▼
运输状态：Start / Stop / Continue
            │
            ▼
I2S 采样时钟 → 96 PPQN 内部时间轴
            │
            ├── 节拍器 click
            ├── 16 步 Pattern
            └── Live Pad 音符
                    │
                    ▼
             多 voice 混音
                    │
                    ▼
                MAX98357A

LED 与 USB 接收时间轴事件，用于显示和同步。
```

网页刷新、USB 收包或 LED 更新都不会改变板端的声音触发时间。断开电脑后，节拍器与鼓机仍可继续运行。

### 2.2 分阶段实现

| 阶段 | 功能目标 | 验收结果 |
| --- | --- | --- |
| P0 | 安全开启共享电源，驱动 RGB 和扬声器 | 灯正常点亮，喇叭播放一声 click |
| P1 | 独立节拍器 | 编码器调 BPM，按键启停，灯光与声音同步 |
| P2 | 八键鼓机与 16 步音序器 | 六种鼓音、Swing、A/B 和 Fill 可用 |
| P3 | 配套软件与主机协议 | 网页可连接、编辑、同步和叠录 |

每个阶段都建立在前一阶段已经验证的硬件与时序基础上。完成当前阶段的验收后，再继续增加下一阶段的功能。

### 2.3 两个独立的播放层

设备中有两个可以分别控制的播放层：

| 播放层 | 上电默认值 | 作用 |
| --- | --- | --- |
| Click | 开启 | 播放节拍器重拍和普通拍 |
| Drum Pattern | 关闭 | 播放 16 步鼓组序列 |

两个播放层可以同时开启。Live Pad 直接触发鼓音，不受 Drum Pattern 开关影响。

---

## 3. 搭建开发环境并烧录固件

### 3.1 准备工作

需要准备：

- EasyInput V2.0 开发板；
- 支持数据传输的 USB-C 线；
- ESP-IDF 5.4.1；
- Node.js 与 pnpm；
- Chrome 或 Edge。

固件目标芯片为 `esp32s3`。

### 3.2 构建固件

```bash
cd firmware
. ~/esp/v5.4.1/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

首次设置 target 后会生成或更新构建配置。后续通常只需执行 `idf.py build`。

### 3.3 进入下载模式

当前开发板使用一键下载电路，操作步骤如下：

1. 保持开发板开机；
2. 短按一次 BOOT 并松开；
3. 等待电脑出现 ESP32-S3 下载端口；
4. 开始烧录。

```bash
idf.py -p /dev/cu.usbmodem* flash
```

烧录完成后，关闭开发板电源，再重新开机。USB RTS 复位可能使开发板停留在下载模式，重新上电后才会运行用户程序。

端口名会随电脑环境变化。脚本和文档中使用端口匹配或设备身份识别，不固定某一个 `usbmodem` 编号。

### 3.4 启动网页应用

回到仓库根目录：

```bash
pnpm install
pnpm dev
```

使用 Chrome 或 Edge 打开终端显示的本地地址。首次连接时，点击页面中的“连接”按钮，并在串口授权窗口中选择 Espressif 设备。

运行网页测试：

```bash
pnpm test
```

### 3.5 桌面应用

桌面版本使用 Electron 封装网页应用：

```bash
# 终端 1
pnpm dev

# 终端 2
pnpm desktop:dev
```

也可以加载已经构建的网页：

```bash
pnpm desktop:start
```

macOS 系统 WebView 不提供 Web Serial，因此桌面封装使用 Electron。

---

## 4. P0：安全上电与第一声 click

P0 用来验证共享电源、RGB 和扬声器链路。此阶段先不加入 BPM 控制、Pattern 和网页同步。

### 4.1 GPIO8 共享电源域

GPIO8 同时控制以下外设的电源：

- 5 颗 WS2812；
- I2S 麦克风；
- MAX98357A 扬声器路径。

冷启动顺序：

1. 将 GPIO8 的输出锁存为低；
2. 将 GPIO9、GPIO10、GPIO12、GPIO13、GPIO14、GPIO15 预设为低电平输出；
3. GPIO11 保持浮空输入，不启用上下拉；
4. 将 GPIO8 拉高；
5. 等待共享电源稳定；
6. 初始化 WS2812 和扬声器 I2S。

这段逻辑位于：

```text
firmware/main/board/board_power.c
```

项目当前使用：

```c
#define BOARD_PWR_SETTLE_MS 50
```

50 ms 是 Beatbox 工程采用的稳定等待时间。开发板资料尚未给出适用于所有项目和批次的硬件最小值，因此修改该参数前需要结合器件规格或实测波形验证。

演奏和显示期间，共享电源保持开启。若需要关闭 GPIO8，应先停止 LED、麦克风和扬声器活动，将相关引脚恢复到安全状态，再切断电源。

### 4.2 点亮 RGB

5 颗 WS2812 通过 GPIO12 串联连接，每帧发送 5 个 GRB 像素。GPIO8 稳定后再发送像素数据。

P0 可以使用一次短暂的 RGB 闪烁确认：

- GPIO12 配置正确；
- 像素数量为 5；
- GRB 顺序正确；
- 共享电源已经稳定。

### 4.3 播放 click

扬声器使用 MAX98357A，I2S 引脚如下：

| 信号 | GPIO |
| --- | ---: |
| BCLK | 14 |
| WS | 13 |
| DATA OUT | 15 |

click 的声音采用短促的 clave / 木块风格：

- 当前长度约 9 ms；
- 主体频率约 2.0–2.5 kHz；
- 重拍与普通拍使用不同音高或音色；
- 高频衰减较快，避免长时间使用时过于刺耳。

实现文件：

```text
firmware/main/audio/audio_click.c
```

### 4.4 P0 验收

- 开机时没有明显爆音；
- RGB 可以正常点亮并熄灭；
- 扬声器可以播放完整、短促的 click；
- GPIO8 开启前没有向下游外设发送数据；
- 关闭共享电源前，相关外设均已停止。

---

## 5. P1：独立节拍器

节拍器需要在未连接电脑时正常工作。设备上电后默认 BPM 为 120，Click 层开启，Drum Pattern 层关闭。

### 5.1 编码器与播放控制

| 操作 | 功能 |
| --- | --- |
| 旋转编码器 | BPM 增减 1 |
| 快速旋转编码器 | BPM 可加速增减 5 |
| 短按编码器 | Play / Stop |
| 短按 S8 | Play / Stop |
| BPM 范围 | 60–240 |

编码器 A/B 是正交相位信号。固件通过相位变化判断旋转方向，不能把两个相位脚当作普通长按按键处理。

### 5.2 运输状态

系统使用三种运输命令：

| 命令 | 行为 |
| --- | --- |
| Start | 将位置归零，从下一音频块开始播放 |
| Stop | 停止调度并保留当前位置 |
| Continue | 从保留位置继续播放 |

板端的编码器按压和 S8 当前执行 Start / Stop。Continue 已在底层运输和主机协议中保留，供电脑控制或未来 MIDI 控制使用。

当前位置包含：

- `bar`：小节编号；
- `beat`：小节内四分拍，范围 0–3；
- `step`：小节内十六分音符，范围 0–15；
- `tick`：小节内 96 PPQN 位置，范围 0–383。

### 5.3 RGB 拍点显示

板上共有 5 颗 LED，显示方式为：

- 4 颗 LED 表示一小节中的四拍位置；
- 1 颗 LED 表示运行、停止或设置状态；
- 第一拍使用更高亮度或不同颜色；
- 拍点动画跟随板端时间事件更新。

完整的 16 步网格由网页显示，板端 LED 保留清晰的四拍提示。

### 5.4 BPM 更新

旋转编码器或接收主机命令后，先将 BPM 限制在 60–240，再更新音频引擎。BPM 变化应保持当前相位连续，避免在调速瞬间重新开始小节或产生重复触发。

相关文件：

```text
firmware/main/seq/tempo.c
firmware/main/main.c
```

### 5.5 P1 验收

分别在 60、120、240 BPM 下运行：

- click 的速度与设定值一致；
- 第一拍重音位置正确；
- LED 与 click 同时更新；
- 调节 BPM 时播放位置连续；
- 断开电脑后节拍器仍能独立运行；
- Start、Stop 和 Continue 的位置行为符合定义。

---

## 6. 音频时钟与时间轴

节拍器、Pattern、Swing、叠录位置和 MIDI Clock 使用同一条板端时间轴。该时间轴由 I2S 音频采样位置派生。

### 6.1 I2S 作为时间源

音频引擎配置：

| 参数 | 数值 |
| --- | --- |
| 采样率 | 32,000 Hz |
| 采样格式 | 16-bit |
| 渲染块 | 128 frame |
| 单块时长 | 4 ms |
| 内部时钟 | 96 PPQN |

音频 DMA 连续运行，每次渲染 128 frame。固件根据已经输出的采样数量累计节拍相位，由此产生 tick、step、beat 和 bar。

普通任务延时受 FreeRTOS 调度、USB 通信、日志和 LED 更新影响，不适合直接推进拍点。项目中的节拍位置统一来自音频采样游标。

### 6.2 PPQN 换算

PPQN（Pulses Per Quarter Note）表示每个四分音符包含多少个时钟脉冲。

| 音乐单位 | 内部 tick |
| --- | ---: |
| 1 个四分音符 | 96 |
| 1 个十六分音符 | 24 |
| 1 小节 4/4 | 384 |

对外 MIDI Clock 为 24 PPQN，因此：

| MIDI 单位 | 与内部时间的关系 |
| --- | --- |
| 1 个 MIDI Clock | 4 个内部 tick |
| 1 个十六分音符 | 6 个 MIDI Clock |
| 1 个四分音符 | 24 个 MIDI Clock |

时钟常量集中在：

```text
firmware/main/seq/clock.h
```

### 6.3 采样位置与 BPM

在给定 BPM 下，每个内部 tick 对应的采样数为：

```text
samples_per_tick = sample_rate × 60 / (BPM × PPQN)
```

例如 120 BPM：

```text
samples_per_tick = 32000 × 60 / (120 × 96)
                 = 166.666...
```

采样数通常不是整数。固件使用高精度相位累计保存小数部分，使长期运行时的误差不会在每个 tick 被截断并不断累积。

### 6.4 Swing

Swing 范围为 50%–75%：

- 50% 表示均匀的十六分音符；
- 奇数编号的十六分音符向后延迟；
- 偶数编号的十六分音符保持原位置；
- 75% 时延迟量达到 12 个内部 tick。

延迟计算：

```c
delay_ticks = (swing - 50) * 12 / 25;
```

Swing 在板端触发时间上生效。网页根据设备状态显示播放位置，不参与声音调度。

### 6.5 音频线程约束

音频线程中保持以下约束：

- 不动态分配内存；
- 不等待网页、USB 或 UI 使用的锁；
- 不直接更新 WS2812；
- 不在每个拍点重新启动 I2S DMA；
- 音符请求通过预先创建的队列进入音频引擎。

这些约束可以减少音频欠载、爆音和拍点抖动。

### 6.6 时间模型验证

运行时间模型检查：

```bash
python scripts/check_timing_model.py
```

脚本覆盖 60–240 BPM 和不同 Swing 值，模型允许误差不超过 1 个 sample。

真机测试还需记录：

- 60、120、240 BPM 下的平均误差；
- 50%、66%、75% Swing 下的触发位置；
- 峰值抖动；
- 约 30 分钟连续运行后的累计漂移。

模型检查用于验证计算逻辑，真机测试用于验证任务调度、I2S、DMA 和实际听感。

---

## 7. 从节拍器扩展为鼓机

鼓机沿用节拍器的音频引擎和时间轴，新增鼓组采样、按键映射和多 voice 混音。

### 7.1 按键映射

| 按键 | 功能 | GM 通道 10 音号 |
| --- | --- | ---: |
| S1 | Closed Hi-Hat | 42 |
| S2 | Open Hi-Hat | 46 |
| S3 | Clap | 39 |
| S4 | Rimshot | 37 |
| S5 | Kick | 36 |
| S6 | Snare | 38 |
| S7 短按 | A/B Variation | — |
| S7 长按 | Fill，松开退出 | — |
| S8 | Play / Stop | — |

上排安排镲片和打击乐，下排安排 Kick 与 Snare，便于使用拇指或食指进行常见的 finger-drumming 演奏。

Closed Hi-Hat 与 Open Hi-Hat 使用 choke 关系。触发 Closed Hi-Hat 时，正在播放的 Open Hi-Hat 应停止或快速衰减。

### 7.2 鼓组采样

默认鼓组使用 `fluid-music/open-drums` 中的 public-domain TR-707 one-shot。资源经过离线处理后嵌入固件：

1. 读取原始采样；
2. 转换为 32 kHz；
3. 转换为 16-bit；
4. 转换为单声道；
5. 导出为 PCM；
6. 在固件构建时嵌入二进制资源。

当前音色包括：

- Kick；
- Snare；
- Closed Hi-Hat；
- Open Hi-Hat；
- Clap；
- Rimshot。

短采样在播放前已经完成格式转换，实时线程不做文件解码。关键 click 和 accent 数据优先放在内部 RAM，更大的音色包可以使用 PSRAM。

### 7.3 多 voice 混音

节拍器、Pattern 和 Live Pad 可能同时触发声音。音频引擎为每个声音分配独立 voice，并在渲染时混合：

```text
click 请求 ─────┐
Pattern 音符 ───┼─→ 请求队列 → voice 分配 → int32 累加 → 限幅 → int16 I2S
Live Pad 音符 ──┘
```

使用 `int32` 累加可以降低多个 `int16` 采样直接相加时的溢出风险。混音完成后进行音量缩放和限幅，再写入 I2S 缓冲。

项目当前预分配 12 个 voice。voice 数量耗尽时，需要使用固定的替换策略，例如优先替换最旧或优先级最低的声音，避免在音频线程中临时申请内存。

### 7.4 Live Pad

按下 S1–S6 后，固件立即向音频请求队列发送对应音符。Live Pad 与运输状态相互独立：

- 运输停止时仍可手动演奏；
- Drum Pattern 关闭时仍可手动演奏；
- Drum Pattern 播放时可以叠加手动演奏；
- 设备向网页回传 `note` 和 `key` 事件。

---

## 8. 16 步音序器与自动播放

### 8.1 Pattern 数据结构

每个 Pattern bank 包含 6 轨、每轨 16 步：

```c
typedef struct {
    uint8_t vel[6][16];
} pattern_bank_t;
```

每格保存一个 velocity：

- `0`：该步不触发；
- `1..127`：触发并作为音符力度。

一张 bank 共 96 字节。项目包含三个 bank：

| Bank | 用途 |
| --- | --- |
| A | 主 Pattern A |
| B | 主 Pattern B |
| Fill | 临时加花 |

轨道顺序固定为：

```text
Kick / Snare / Closed HH / Open HH / Clap / Rim
```

### 8.2 步进触发

一小节包含 16 个十六分音符，每步相隔 24 个内部 tick。音频引擎到达新的 step 后，音序器执行以下流程：

1. 确定当前 bank；
2. 读取 6 条轨道在当前 step 的 velocity；
3. 对非零格生成音符请求；
4. 将请求送入同一个音频混音器；
5. 向主机发送当前位置。

Swing 开启时，奇数 step 使用延迟后的触发位置。

### 8.3 A/B Variation

短按 S7 请求切换 A/B。切换在下一个十六分音符边界生效，使两个 Pattern 保持在同一网格上。

固件保存：

- 当前 Variation；
- 待生效的 Variation；
- 当前 step；
- 切换生效后的状态回报。

### 8.4 Fill

长按 S7 进入 Fill，松开后返回此前的 A 或 B。Fill 只临时覆盖当前播放 bank，不改变 A/B 的选择。

### 8.5 Pattern revision

Pattern 使用 `rev` 标识修订版本。网页提交修改时携带已知 revision，设备接受修改后增加 revision，并返回确认。

设备保存当前有效 Pattern。网页可以维护未提交的本地草稿；完成提交或重新同步后，以设备返回的 Pattern 和 revision 更新界面。

当前 MVP 的 `save` 命令用于更新 revision 和返回确认。若需要断电保存，应继续实现 NVS 写入、读取和版本迁移。

---

## 9. USB Serial 与网页联动

### 9.1 通信方式

当前主通道为：

```text
ESP32-S3 USB Serial/JTAG ↔ Web Serial ↔ 浏览器应用
```

协议使用 UTF-8 文本，每行一条 JSON，以 `\n` 结尾。协议版本通过 `hello.v = 2` 标识。

串口打开后，网页等待设备发出：

1. `hello`：设备身份、协议版本和能力；
2. `state`：BPM、运行状态、位置、Swing、Pattern revision 等状态。

两条消息都有效后，连接状态进入 `synced`。

### 9.2 自动重连

首次连接需要用户在浏览器中授权串口。授权后，网页可以通过：

```ts
navigator.serial.getPorts()
```

找到此前授权的设备并尝试重连。连接逻辑优先识别 Espressif VID `0x303A`，随后通过 `hello` 验证设备身份和协议版本。

### 9.3 设备发往网页的主要消息

| 消息 | 用途 |
| --- | --- |
| `hello` | 身份、协议版本和能力 |
| `state` | 完整状态快照 |
| `position` | bar、step、beat、tick |
| `beat` | 四分拍事件 |
| `note` | Live Pad 或序列音符 |
| `key` | 物理按键按下与释放 |
| `pattern` | 单个 bank 的 Pattern |
| `start` / `continue` / `stop` | 运输状态变化 |
| `ack` | 命令执行确认 |
| `error` | 命令错误 |

示例：

```json
{"t":"state","bpm":120,"run":1,"beat":0,"step":0,"bar":2,"tick":0,"swing":50,"var":0,"fill":0,"rev":3,"click":1,"mode":1,"vol":100}
```

### 9.4 网页发往设备的主要消息

| 消息 | 用途 |
| --- | --- |
| `start` / `continue` / `stop` | 运输控制 |
| `bpm` | 设置 BPM |
| `swing` | 设置 Swing |
| `variation` | 请求 A/B 切换 |
| `fill` | 进入或退出 Fill |
| `note` | 网页 Live Pad |
| `click` | Click 层开关 |
| `mode` | Drum Pattern 层开关 |
| `volume` | 总音量 |
| `pattern_get` | 请求 Pattern |
| `pattern_set` | 提交单个 bank |
| `record` | 叠录武装 |
| `ping` | 请求重新发送身份和状态 |

示例：

```json
{"t":"bpm","v":128}
{"t":"swing","v":66}
{"t":"mode","v":1}
```

完整字段定义见 `docs/host-protocol.md`。

### 9.5 网页界面

网页分为两个主要区域：

- 节拍器与运输控制：BPM、播放、Swing、音量、Click 和 Drum Pattern 开关；
- 鼓机编辑器：16 步 × 6 轨 Pattern、A/B/Fill 和 Live Pad。

播放位置由设备的 `position` 和 `beat` 消息更新。浏览器计时器只用于界面辅助，不生成板端拍点。

---

## 10. Pattern 叠录

本项目中的录音功能指 Pattern 叠录（overdub）。它记录鼓垫演奏发生在哪一个十六分音符位置，不采集麦克风音频。

### 10.1 叠录流程

1. 在网页中选择 A 或 B；
2. 启动运输；
3. 点击 REC；
4. 使用板端鼓垫或网页 Live Pad 演奏；
5. 音符写入当前播放位置对应的轨道与 step；
6. 再次点击 REC 结束叠录；
7. 将修改后的 bank 提交给设备。

已有格子会被保留，新演奏叠加到当前 Pattern。Fill bank 不提供 REC。

### 10.2 录音武装

网页开始录音时发送：

```json
{"t":"record","v":1}
```

结束录音时发送：

```json
{"t":"record","v":0}
```

录音武装期间，设备暂时忽略以下控制：

- S7 的 A/B 和 Fill；
- S8 的播放控制；
- 编码器按压的播放控制。

S1–S6 仍然可以演奏和记录。锁定运输相关按键可以避免录音过程中误停或切换 bank。

### 10.3 音符量化

网页根据设备报告的 `step` 和运行状态记录音符。每个音符映射到对应鼓轨，并把 velocity 写入当前 bank 的相应格子。

时间位置来自板端。USB 传输存在延迟，因此录音实现需要使用设备回报的位置和明确的量化策略，不能把浏览器收到事件的本地时间直接当作音频触发时间。

### 10.4 导入与导出

Pattern 支持 JSON 导入和导出，数据包括：

- A bank；
- B bank；
- Fill bank；
- 每格 velocity；
- 必要的格式版本信息。

导入后，网页通过 `pattern_set` 分 bank 提交，设备更新 revision 并返回确认。

---

## 11. MIDI 语义

### 11.1 Serial 协议与 MIDI

当前 USB 链路传输 JSON 领域消息。项目同时定义 MIDI 映射，使运输和鼓音可以接入标准音乐软件。

| 领域事件 | MIDI 消息 |
| --- | --- |
| Start | `0xFA` |
| Continue | `0xFB` |
| Stop | `0xFC` |
| Clock | `0xF8`，24 PPQN |
| step 位置 | Song Position Pointer |
| 鼓音 | 通道 10 Note On / Note Off |

鼓音映射：

| 音色 | MIDI Note |
| --- | ---: |
| Kick | 36 |
| Rimshot | 37 |
| Snare | 38 |
| Clap | 39 |
| Closed Hi-Hat | 42 |
| Open Hi-Hat | 46 |
| 重拍 click | 76 |
| 普通拍 click | 77 |

适配代码位于：

```text
app/src/midi-adapter.ts
```

适配层使用纯函数完成领域事件与 MIDI 字节之间的转换，便于单元测试，也便于将来替换传输方式。

### 11.2 设备管理命令

以下数据没有通用的标准 MIDI CC 定义，继续通过 Serial JSON 管理：

- BPM；
- Swing；
- Pattern get/set；
- Pattern revision；
- Click 开关；
- Drum Pattern 开关；
- 总音量；
- 保存命令。

项目不再使用历史草案中的 CC16/CC17 BPM 编码。

### 11.3 USB MIDI Class 的当前状态

ESP32-S3 的 USB-OTG 与 USB-Serial/JTAG 共用一套 PHY。当前开发流程依赖 USB Serial/JTAG 完成烧录和主机通信；在 macOS 上切换到 TinyUSB MIDI 还需要解决枚举和恢复稳定性。

在以下项目全部通过前，USB Serial 保持为默认链路：

- MIDI 与串口能够稳定枚举；
- BOOT、烧录和重新上电后可以恢复；
- Start、Stop、Continue 双向一致；
- 24 PPQN Clock 长时间运行稳定；
- GM 通道 10 音符无重复触发；
- 日常烧录和网页连接不受影响。

详细测试清单见 `docs/usb-midi-spike.md`。

需要提前连接 DAW 时，可以增加桌面桥接程序，将 Serial 领域事件转换为系统虚拟 MIDI 端口。

---

## 12. 测试与验收

### 12.1 板级基线

运行：

```bash
./scripts/check_board_baseline.sh
```

检查内容包括按键、编码器、GPIO8、LED 和音频引脚声明。脚本通过表示未发现已经建模的引脚冲突，仍需结合构建、烧录和实物测试。

### 12.2 固件构建

```bash
cd firmware
. ~/esp/v5.4.1/esp-idf/export.sh
idf.py build
```

确认：

- 编译和链接完成；
- PCM 资源已经嵌入；
- Flash 和 RAM 使用量处于可接受范围；
- 没有音频任务栈溢出警告。

### 12.3 网页测试

```bash
pnpm test
```

重点覆盖：

- 协议消息解析；
- 设备状态更新；
- Pattern 编解码；
- MIDI 适配；
- revision 与同步状态。

### 12.4 真机功能检查

| 模块 | 检查项 |
| --- | --- |
| 上电 | 无爆音；RGB 和扬声器正常 |
| 节拍器 | 60、120、240 BPM 正确 |
| 编码器 | 方向、步进、快速旋转和按压正常 |
| LED | 第一拍、四拍位置和运行状态正确 |
| Live Pad | 六种音色映射正确；无明显截断 |
| Pattern | 16 步、A/B、Fill 正确 |
| Swing | 50%、66%、75% 听感与位置正确 |
| 叠录 | 保留已有格子；按键锁定正确 |
| USB | 首次授权、断开、重连和同步正常 |
| 长时间运行 | 约 30 分钟无明显漂移、爆音或断连 |

---

## 13. 常见问题

### 13.1 烧录成功后程序没有运行

开发板可能仍停留在下载模式。关闭电源，再正常开机。恢复运行时无需再次按 BOOT。

### 13.2 电脑没有出现下载端口

依次检查：

1. 开发板是否已经开机；
2. USB-C 线是否支持数据；
3. 是否短按并松开了 BOOT；
4. 电脑的 USB 端口列表是否发生变化；
5. 是否连接了正确的 EasyInput 开发板。

### 13.3 LED 正常但扬声器无声

检查：

- GPIO8 是否已经拉高并等待稳定；
- MAX98357A 的 BCLK、WS、DATA OUT 是否为 GPIO14、13、15；
- I2S 是否持续写入；
- Click 或 Drum Pattern 是否被关闭；
- 总音量是否为 0。

### 13.4 扬声器正常但 LED 异常

检查：

- WS2812 数据脚是否为 GPIO12；
- 像素数量是否为 5；
- 颜色顺序是否为 GRB；
- GPIO8 是否保持开启；
- LED 更新是否放在音频线程之外。

### 13.5 拍点偶尔抖动

检查声音触发是否直接来自音频采样时间轴。USB、网页计时器、LED 任务和普通 `vTaskDelay()` 都不应推进节拍位置。

同时检查：

- 音频线程是否动态分配内存；
- 是否等待了其他模块的锁；
- 日志量是否过大；
- DMA 是否发生欠载；
- 音符队列是否溢出。

### 13.6 网页已经打开串口但没有同步

串口打开只表示传输通道已经建立。网页还需要收到合法的 `hello` 和 `state`。

检查：

- 协议版本是否为 2；
- JSON 是否以换行结束；
- 非 JSON 日志是否混入协议行；
- 设备是否响应 `ping`；
- 网页选择的端口是否为目标设备。

### 13.7 Pattern 修改被覆盖

检查网页提交时携带的 revision，以及设备返回的最新 revision。重新获取 Pattern 后，以设备中的有效数据更新网页草稿，再继续编辑。

---

## 14. 后续扩展

当前架构可以继续支持：

- Pattern 写入 NVS，实现断电保存；
- 更多鼓组和用户音色包；
- PSRAM 中的大型采样；
- Serial 到虚拟 MIDI 的桌面桥接；
- 通过完整验收后的 USB MIDI Class；
- 外部 MIDI Clock 从机模式；
- 麦克风录音或采样。

麦克风与扬声器同时使用时，需要重新验证 I2S 控制器分配、时钟、DMA、共享电源和任务负载。

---

## 15. 相关文件

| 内容 | 文件 |
| --- | --- |
| 开发板资料 | `.cursor/skills/easyinput-board-cy/` |
| Beatbox 产品约定 | `.cursor/skills/easyinput-drum-machine/SKILL.md` |
| 产品合同 | `docs/product-contract.md` |
| 主机协议 | `docs/host-protocol.md` |
| MIDI 语义 | `docs/midi-protocol.md` |
| USB MIDI 测试清单 | `docs/usb-midi-spike.md` |
| 引脚常量 | `firmware/main/board/board_pins.h` |
| 共享电源 | `firmware/main/board/board_power.c` |
| 音频引擎 | `firmware/main/audio/audio_click.c` |
| 时钟常量 | `firmware/main/seq/clock.h` |
| Pattern | `firmware/main/seq/pattern.c` |
| 音序器 | `firmware/main/seq/sequencer.c` |
| 主机链路 | `firmware/main/host/host_link.c` |
| 网页叠录 | `app/src/main.ts` |
| MIDI 适配 | `app/src/midi-adapter.ts` |
