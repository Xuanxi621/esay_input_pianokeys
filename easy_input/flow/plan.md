# 计划 (plan) —— 契约

> 经确认后执行。要偏离,**先改这里**再动手。

## 里程碑
- [x] M0: 接入 project-flow-cy 多 Agent 协作骨架，建立总体控制面与子项目局部入口
- [ ] M1: 梳理并建立软硬件引脚契约 (`docs/contracts/pinout.md`) 与工具链环境基线
- [x] M2: 验证子模块联调与构建全流程 (ESP-IDF 5.5.5 + easy-input-maker 固件编译 + host_test)

## 任务拆解
| 任务 | 负责角色 / 工具 | 输入 | 产出(落哪个文件) | 验收标准 |
|---|---|---|---|---|
| T01: 初始化协作流程 | AI Agent | project-flow-cy 规范 | `flow/`, `docs/`, `AGENTS.md`, `CLAUDE.md` | 骨架完备，自检通过 |
| T02: 建立软硬件契约 | AI Agent / 开发者 | easyinput-board-cy / easy-input-maker | `docs/contracts/hardware-firmware-contract.md` | GPIO/电源规范统一 |
| T03: 工具链环境验证 | AI Agent | eim_config.toml / esp-idf-cy | `docs/modules/esp-idf-cy.md` | 本地或 CI 可一键调通 |

## 实时进展 / 交接棒
→ 见 `flow/进展.md` 顶部(每棒收工在那追加一条:做了什么 / 为什么 / 产出路径 / 下一步)。
（plan.md 只管"计划=契约";"现在到哪了"在进展日志,不在这儿覆盖。）
