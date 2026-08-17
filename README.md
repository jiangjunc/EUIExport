# EUIExport（蛋仔派对 EUI 工具箱）

[![npm](https://img.shields.io/npm/v/euiexport)](https://www.npmjs.com/package/euiexport)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub](https://img.shields.io/badge/GitHub-jiangjunc%2FEUIExport-blue)](https://github.com/jiangjunc/EUIExport)

> 蛋仔派对 PC 编辑器 EUI 工具的合并版 CLI：**导出** + **字段编辑**，一个命令搞定。
> 零运行时依赖（内置 Zstandard + MessagePack 编解码）。
> 同时支持 **FS（帧同步）** 与 **SE（世界版 / 状态同步）** 两类地图。
>
> **适用对象**：蛋仔派对 PC 编辑器（Eggitor）的地图工程开发者。
>
> 源码仓库：<https://github.com/jiangjunc/EUIExport>
> npm 主页：<https://www.npmjs.com/package/euiexport>

## 安装

```bash
# 全局安装，装完直接敲 eui 命令
npm install -g euiexport

# 或者不装，用 npx 临时跑一把
npx euiexport export 我的地图
```

> 装完 `eui -h` 看看；想从源码跑就用 `node bin/eui.js ...`。
> 找草稿需要蛋仔派对 PC 编辑器（Eggitor）已安装（自动从 `~/.eggitor/cli/editor_config.json` 定位安装目录）。

## 它能做什么

蛋仔编辑器的 UI（EUI）存在地图工程的 `eui.mm`（FS）/ `euidata.mm`（SE）里，内部是 **Zstandard 压缩 + MessagePack 序列化** 的二进制数据，非常不利于人类和AI阅读，本项目工具完全解决这一痛点，让AI可以"看"见UI页面。

本工具提供两坨主要作用：

| 子命令 | 作用 |
| --- | --- |
| `eui export` | 把 EUI 数据还原成 **AI 可读**的 UI 数据包（JSON + 树状图 + 字段字典），让 AI / 脚本直接"看到"界面 |
| `eui edit` | **直接修改 EUI 字段**——包括官方 Lua API 无法修改的属性（Meta 只读 / 私有字段） |

> **FS / SE 地图**：两种类型的 EUI 数据 schema 完全是一个一毛一样的玩意（FS 用 `eui.mm`，SE 用 `euidata.mm`），工具自觉识别，不用你瞎操心。
> 自动扫描的目录：
>- `editor_maps`（FS地图数据藏身地）
>- `help_build_gmps`（可能包含SE地图数据藏身地）
>- `joint_construction_gmps`（共建地图数据藏身地）
>- `se_maps`（SE草稿地图数据藏身地）。

## 快速开始

```bash
# 交互式选一坨草稿并导出(会列出自动从各个角落找到的草稿名称给你自助餐吃)
eui export

# 按草稿名称导出（支持仅输入半个地图名称）
eui export 我的地图

# 指向编辑器的地图本地数据存储目录 或者 base64 地图id
# 提个醒: ~ 是编辑器安装目录(可通过`网易发烧友平台=>蛋仔派对"开始游戏"按钮右边的三条横杠=>查看文件`进行获取编辑器安装路径)
# 编辑器的地图本地数据存储目录: 一般编辑器扔在`~/Documents/etc/editor_maps/`
# base64 地图id: 从你Lua代码项目里的 `eggy.json` 里的 `projectID` 字段获取
eui export "~/Documents/etc/editor_maps/<base64id>__pc"
eui export <base64id> -c

# 读写UI字段属性
eui edit 我的地图 --dump # 查看所有字段
eui edit 我的地图 --set "nodes[0].size[0]=100" --in-place # 修改第一个节点的宽度为100px

# 列出全部草稿 / 查看类型对照表
eui list
eui types
```

本地开发时（没全局安装）用 `node bin/eui.js export 我的地图` 效果一毛一样。

## CLI 命令

```
eui export <草稿名称|地图目录|地图id> [选项]   导出 UI 数据包
eui edit   <草稿名称|地图目录|地图id> [选项]   修改 eui.mm 字段
eui list [选项]                              列出所有地图草稿
eui types                                   打印控件类型对照表
eui config [set|get|unset|path]             管理配置（默认导出路径等）
eui -h | --help                             显示帮助
eui -v | --version                          显示版本号
```

> 兼容旧用法：`eui <草稿名称|目录|id> [选项]` 等价于 `eui export ...`。

### `export` 选项

| 参数 | 说明 |
| --- | --- |
| `<草稿名称>` | 按地图草稿名称获取（支持片段） |
| `<地图目录>` | 含 `eui.mm` 的 `xxx__pc` 文件夹；也支持 `editor_maps` 根目录 / base64 地图 id / 路径片段 |
| `-l, --list` | 列出所有地图草稿名称后退出 |
| `-i, --interactive` | 强制进入交互选择（脚本 / 管道也可用） |
| `-r, --root <path>` | 手动指定编辑器根目录（自动探测失败时用） |
| `-o, --output <dir>` | 输出目录（直接指定）；未指定时按优先级取：`EGGY_EUI_OUTPUT` 环境变量 > `eui config export-dir` > 当前目录 |
| `-c, --compact` | UIData.json 输出压缩 JSON（默认美化缩进 2 格） |
| `-t, --list-types` | 打印控件类型对照表后退出 |
| `-q, --quiet` | 安静模式：只打印一行结果摘要 |
| `--no-color` | 禁用 ANSI 颜色 |

### 设置默认导出路径

不想每次敲 `-o`？两种方式可持久设置默认导出路径，之后所有导出自动写入 `<导出路径>/<草稿名>/`（除非本次用 `-o` 覆盖）：

```bash
# 方式一：配置文件（推荐，写入 ~/.euiexport.json）
eui config set export-dir D:/EUI_OUTPUT
eui config get export-dir        # 查看
eui config unset export-dir      # 删除
eui config path                  # 查看配置文件位置

# 方式二：环境变量（适合脚本 / CI，优先级高于配置文件）
set EGGY_EUI_OUTPUT=D:/EUI_OUTPUT    # Windows
export EGGY_EUI_OUTPUT=/data/eui     # macOS / Linux
```

**导出目录优先级**：`-o 参数` > `EGGY_EUI_OUTPUT 环境变量` > `eui config export-dir 配置` > 当前目录。
其中 `-o` 是"直接指定目录"（文件直接落进去）；后两者是"基础目录"（实际输出 `<基础目录>/<草稿名>/`）。

> 配置文件 `~/.euiexport.json` 属于用户级配置，不含任何地图数据，可放心配合 Git 使用；也可用环境变量 `EGGY_EUI_CONFIG` 指定其他配置文件路径（多环境 / CI 场景）。

### `edit` 选项

| 参数 | 说明 |
| --- | --- |
| `--set <path>=<value>` | 设置 / 新增字段，可多次（例：`nodes[标题].size[0]=100`） |
| `--patch <file.json>` | 应用 JSON Patch 数组 `[{op,path,value}]`，op: `replace` \| `add` \| `remove` |
| `--dump` | 打印场景顶层字段 + 节点字段样本（用于找路径） |
| `-l, --list` | 列出所有地图草稿 |
| `--in-place` | 直接写回原 `eui.mm`（自动备份 `eui.mm.bak`） |
| `-o, --output <file>` | 输出到指定文件（默认: `<地图目录>/eui.modified.mm`） |
| `-r, --root <path>` | 手动指定编辑器根目录 |
| `-q, --quiet` `--no-color` `-h, --help` | 通用选项 |

**路径语法**：数组用 `[下标]`（或 `.下标`）；节点可用「名称」或「id」选择；嵌套对象用 `.` 逐层进。`--set` 值能按 JSON 解析就按 JSON（数字 / 布尔 / 数组 / 对象），否则按字符串。

## 草稿名称从哪来

每个地图工程 `desc.mm` 里的 `map_name` 字段（权威来源）；读不到时回退到 `Documents/vscode_projs.json`（VS Code 工程名）。

**扫描范围**：自动遍历 FS 与 SE 的所有地图工程根目录（`editor_maps` / `help_build_gmps` / `joint_construction_gmps` / `se_maps`），并识别 FS（`eui.mm`）与 SE（`euidata.mm`）两种工程；同一地图存在于多个根目录时按地图 UUID 去重。

## `export` 输出内容

输出是一个文件夹（默认以草稿名命名），包含三个文件：

| 文件 | 作用 |
| --- | --- |
| `UIData.json` | 完整节点数据：`summary` 统计 + `node_index` 快速索引（id → 名称/类型/路径）+ `scene` 完整 UI 树 |
| `UITree.md` | 完整节点树状图索引（文本结构树，每行 `「名称」 [类型 #id]` + 关键信息） |
| `README.md` | AI 阅读指南 + 全部字段含义字典（建议 AI 第一个读它） |

`UIData.json` 顶层结构：

```json
{
  "format": "eggy-eui-export",
  "schema_version": 2,
  "map_name": "<地图草稿名>",
  "map_id": { "base64": "...", "hex": "..." },
  "summary": { "node_count": 114, "canvas_count": 4, "max_depth": 5, "joystick_count": 28, "type_counts": { ... } },
  "node_index": { "<节点id>": { "name": "标题", "type": "Text", "path": "主界面 > 标题" } },
  "scene": {
    "type": "Scene",
    "version": "0.0.1",
    "nodes": [ ... 根画布节点树 ... ]
  }
}
```

`scene.nodes` 里每个节点对象包含可读分组：`layout`（布局几何）、`visibility`（显隐规则）、`events`（触摸与显隐事件）、`behavior`（交互）、`text/image/panel/background`（控件专属外观，颜色已转 `#hex`）、`children`（子节点数组）、`other`（未归类原始字段）。

## 原理（存储链路）

```
EUI 数据（FS: eui.mm / SE: euidata.mm）
  └─ Zstandard 解压 ──→ MessagePack 解码 ──→ 节点树（export：可读化；edit：改字段后重编码压缩写回）
```

- **export**：EUI 数据 → zstd 解压 → msgpack 解码 → 可读化节点树 → 写 3 个文件
- **edit**：EUI 数据 → zstd 解压 → msgpack 解码 → 改字段 → msgpack 重编码 → zstd 压缩 → 写回

> **注意**：编辑器不会自动监听 `eui.mm` / `euidata.mm`。编辑后需重载地图：关图重开，或 `editor-cli map reopen --force`（会丢弃未保存的内存修改）。

## 环境要求

- Node.js >= 16
- 蛋仔派对 PC 编辑器（Eggitor）安装目录（用于自动发现地图草稿）

## 项目结构

```
eui-cli/
├── bin/eui.js           # 单一入口（子命令分发）
├── lib/
│   ├── cmd-export.js    # export 子命令
│   ├── cmd-edit.js      # edit 子命令
│   ├── cmd-config.js    # config 子命令（默认导出路径等）
│   ├── config.js        # 用户配置读写（~/.euiexport.json）
│   ├── eui.js           # EUI 节点可读化 / 统计 / 树索引
│   ├── maps.js          # 地图草稿发现与选择
│   ├── readme.js        # 输出包内 README（AI 阅读指南）生成
│   ├── msgpack.js       # MessagePack 解码/编码
│   ├── zstd.cjs         # Zstandard 解压（wasm）
│   └── zstd-codec/      # Zstandard 压缩（asm.js，edit 用）
├── test/
│   ├── smoke.js         # 导出流水线冒烟测试
│   └── edit-smoke.js    # 编辑往返保真冒烟测试
└── package.json
```

## 测试

冒烟测试需要指向一个真实地图工程：

```bash
node test/smoke.js <地图工程目录>
node test/edit-smoke.js <地图工程目录>
```

## License

MIT