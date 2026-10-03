# USB MIDI Class 可行性 Spike

本文件是验收清单，不是当前默认链路。默认仍为 USB-Serial + Web Serial。

## 目标

在不破坏烧录/串口调试的前提下，评估 ESP32-S3 上 TinyUSB MIDI（或 CDC+MIDI 复合设备）是否可在 macOS 稳定枚举。

## 前置条件

- 领域事件与 MIDI 适配层已通过单元测试（`app/src/midi-adapter.ts`）
- Serial JSON v2 鼓机链路已可用
- 板端 96 PPQN 与 24 PPQN 导出关系已冻结

## 验收门槛（全部通过才可切换默认传输）

1. **枚举**：插上后 macOS Audio MIDI Setup 出现 `EasyInput Beatbox`，且 `cu.usbmodem*` / CDC 仍可用于烧录或调试之一
2. **恢复**：短按 BOOT 下载 → 烧录 → 断电再开机后 MIDI 与串口均可恢复，无需烧 eFuse
3. **运输**：DAW Start/Stop/Continue 与板端双向一致
4. **Clock**：120 BPM 下 30 分钟；接收侧统计 24 PPQN，平均误差与峰值抖动可接受（目标：视觉/听感无漂移）
5. **Note**：Ch.10 36/38/42/46/39 触发板端对应音色，无双触发
6. **回归**：配套 Web Serial UI 仍能连接（若 PHY 互斥则明确降级策略，不得静默丢失烧录口）

## 失败时的退路

- 保持 Serial 为主通道
- 可选原生桌面桥接器：Serial ↔ 虚拟 MIDI 端口
- **不要**把 WebMIDI 说成“网页自己变成 DAW 虚拟设备”

## 当前状态

未执行真机 spike。适配层与协议合同已就绪，等待独立硬件验证窗口。
