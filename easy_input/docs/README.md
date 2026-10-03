# docs/ —— 集中内容层

项目需要统一查找的**实际内容产出**写在这里：产品需求、调研、架构、跨模块契约、设计说明、评审和交付说明……

- 单仓多子项目共用这一份根级 `docs/`，按需建 `product/`、`architecture/`、`contracts/`、`modules/<module>/`、`reviews/` 等子目录。
- 控制层（charter / plan / 任务卡 / 交接卡）不放这里，放 `flow/`。
- 子项目目录默认放代码、测试和构建配置，不主动重复建立文档体系。
- 已有 README、生成文档或必须紧贴代码维护的说明留在子项目里；在下方索引其真实路径，不要为了集中而搬动。
- 判据：需要**统一发现的知识、方案和交付说明** → 根级 `docs/`；**协调 / 推进项目** → 根级 `flow/`；代码 → 对应子项目。

## 局部文档索引

- `easy-input-maker/README.md` — EasyInput Maker 开源固件架构与模块开发指南
- `easy-input-maker/AI_DEVELOPMENT.md` — 固件开发原则与 AI 贡献指引
- `easy-input-maker/docs/hardware/easyinput-v2-safety.md` — 硬件安全与电气规范（引脚、BOOT、GPIO8 电源生命周期）
- `easy-input-maker/docs/getting-started/` — 固件编译、烧录与调试指引
- `easyinput-board-cy/README.md` — EasyInput 硬件载板、原理图与 PCB 设计参考规范
- `easyinput-board-cy/DESIGN.md` — 硬件设计理念、器件选型与板级参数
- `esp-idf-cy/README.md` — ESP-IDF 5.5.5 工具链配置与多平台环境维护指引
- `eim_config.toml` — 根目录工具链与 Python/ESP-IDF 下载源配置
