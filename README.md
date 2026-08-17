# EUIExport（蛋仔派对 EUI 工具箱）

[![npm](https://img.shields.io/npm/v/euiexport)](https://www.npmjs.com/package/euiexport)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub](https://img.shields.io/badge/GitHub-jiangjunc%2FEUIExport-blue)](https://github.com/jiangjunc/EUIExport)

> 蛋仔派对 PC 编辑器 EUI 工具 CLI：**导出** + **字段编辑**，一个命令搞定。
> 命令行参数解析基于 [commander](https://github.com/tj/commander.js)，交互选择基于 [enquirer](https://github.com/enquirer/enquirer)，编解码（Zstandard + MessagePack）全部内置。
> 同时支持 **FS（帧同步）** 与 **SE（世界版 / 状态同步）** 两类地图。
>
> **适用对象**：蛋仔派对 PC 编辑器（Eggitor）的地图工程开发者。
>
> 源码仓库：<https://github.com/jiangjunc/EUIExport>
> npm 主页：<https://www.npmjs.com/package/euiexport>

## 特性

蛋仔编辑器的 UI（EUI）存在地图工程的 `eui.mm`（FS）/ `euidata.mm`（SE）里，内部是 **Zstandard 压缩 + MessagePack 序列化** 的二进制数据，非常不利于人类和 AI 阅读。本工具把 EUI 还原成 AI 可读的数据，并允许直接修改存档字段。

| 子命令 | 作用 |
| --- | --- |
| `eui export` | 把 EUI 数据还原成 **AI 可读**的 UI 数据包（JSON + 树状图 + 字段字典），让 AI / 脚本直接"看到"界面 |
| `eui edit` | **直接修改 EUI 字段**——包括官方 Lua API 无法修改的属性（Meta 只读 / 私有字段） |

> **FS / SE 地图**：两种类型的 EUI 数据 schema 完全相同（FS 用 `eui.mm`，SE 用 `euidata.mm`），工具自动识别类型。
> 自动扫描的目录：`editor_maps`（FS 地图数据）、`help_build_gmps`（官方教学）、`joint_construction_gmps`（共建）、`se_maps`（SE 草稿）。

## 安装

```bash
# 全局安装，装完直接敲 eui 命令
npm install -g euiexport

# 或者不装，用 npx 临时跑一把
npx euiexport export 我的地图
```

> 装完 `eui -h` 看看；想从源码跑就先 `npm install`（会装 commander），再 `node bin/eui.js ...`。
> 找草稿需要蛋仔派对 PC 编辑器（Eggitor）已安装。编辑器根目录按优先级确定：
> `-r/--root` 命令行参数 > `eui config set editor-root <path>` 配置 > 编辑器官方配置 `~/.eggitor/cli/editor_config.json` > 当前工作目录。

## 快速开始

```bash
# 交互式选择草稿并导出
eui export

# 按草稿名称导出（支持: 地图全名 / 前缀匹配 / 关键词匹配）
eui export 我的地图

# 指向编辑器的地图本地数据存储目录 或者 base64 地图id
# 提个醒: cwd是编辑器安装目录(可通过`网易发烧友平台=>蛋仔派对"开始游戏"按钮右边的三条横杠=>查看文件`进行获取编辑器安装路径)
# 编辑器的地图本地数据存储目录: 一般编辑器扔在`./Documents/etc/editor_maps/`
# base64 地图id: 从你Lua代码项目里的 `eggy.json` 里的 `projectID` 字段获取
eui export "./Documents/etc/editor_maps/<base64id>__pc"
eui export <base64id> -c

# 读写UI字段属性
eui edit 我的地图 --dump # 查看所有字段
eui edit 我的地图 --set "nodes[0].size[0]=100" --in-place # 修改第一个节点的宽度为100px

# 列出全部草稿 / 查看类型对照表
eui list
eui types
```

本地开发时（没全局安装）用 `node bin/eui.js export 我的地图` 效果一毛一样。

> **编辑路径语法**：`--set <path>=<value>` 里，数组用 `[下标]`（或 `.下标`）；节点可用「名称」或「id」选择；嵌套对象用 `.` 逐层进；值能按 JSON 解析就按 JSON（数字 / 布尔 / 数组 / 对象），否则按字符串。完整字段路径可用 `eui edit <草稿> --dump` 查看。

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
> 每个子命令的完整选项以 CLI 帮助为准：`eui export -h` / `eui edit -h` / `eui config -h` / `eui list -h`。
> 交互选择（如 `eui export` 无参数时）支持方向键 ↑↓ + 回车（基于 enquirer）。

## 配置

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

**导出目录优先级**：`-o 参数` > `EGGY_EUI_OUTPUT 环境变量` > `eui config export-dir 配置` > 草稿对应的 lua 工程目录 > 当前目录。
其中 `-o` 是"直接指定目录"（文件直接落进去）；其余是"基础目录"（实际输出 `<基础目录>/<草稿名>/`）。
> 默认（未指定 -o / 环境变量 / 配置）时，导出到**该草稿对应的 lua 工程目录**（如 `LuaSource_记忆卡牌/记忆卡牌/`）；找不到 lua 工程时才回退当前目录。

### 设置编辑器根目录

工具靠「编辑器根目录」来发现地图草稿（`editor_maps` 等目录都在编辑器安装目录下）。查找优先级（高 → 低）：

1. `-r, --root <path>` 命令行参数（本次生效）
2. `eui config set editor-root <path>` 配置文件（持久化，推荐）
3. 编辑器官方配置 `~/.eggitor/cli/editor_config.json`（`project_dir` / `client_exe`）
4. 当前工作目录

```bash
# 持久化指定编辑器根目录（网易发烧友平台：蛋仔派对「开始游戏」旁 ≡ => 查看文件 即安装目录）
eui config set editor-root D:/FeverApps/party_pc
eui config get editor-root        # 查看
eui config unset editor-root      # 删除

# 或本次临时指定
eui export -r D:/FeverApps/party_pc 我的地图
```

> 配置文件 `~/.euiexport.json` 属于用户级配置，可放心配合 Git 使用；也可用环境变量 `EGGY_EUI_CONFIG` 指定其他配置文件路径（多环境 / CI 场景）。

## 进阶

### 草稿名称从哪来

每个地图草稿的名称按优先级获取：

1. **编辑器 mm 文件**：工程 `desc.mm` 里的 `map_name` 字段（权威来源）。
2. **lua 工程 `eggy.json` 的 `projectName`**：从草稿的 `.gmp` 文件读出工程 ID，在编辑器 `Documents/vscode_projs.json` 记录的 lua 工程目录（`lua_root`）中定位对应工程，取该工程 `eggy.json` 的 `projectName`。
3. **`Documents/vscode_projs.json`**：VS Code 工程名（从 `LuaSource_<名>` 目录名提取）。

**扫描范围**：自动遍历 FS 与 SE 的所有地图工程根目录（`editor_maps` / `help_build_gmps` / `joint_construction_gmps` / `se_maps`），并识别 FS（`eui.mm`）与 SE（`euidata.mm`）两种工程；同一地图存在于多个根目录时按地图 UUID 去重。

### `export` 输出内容

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

### 原理（存储链路）

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
EUIExport/
├── bin/eui.js           # 单一入口（commander 根程序 + 子命令组装/分发）
├── lib/
│   ├── cmd-export.js    # export 子命令（commander 定义选项）
│   ├── cmd-edit.js      # edit 子命令（commander 定义选项）
│   ├── cmd-config.js    # config 子命令（commander 定义 path/get/set/unset）
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
└── package.json         # 依赖：commander（参数解析）+ enquirer（交互选择）
```

## 测试

冒烟测试需要指向一个真实地图工程：

```bash
node test/smoke.js <地图工程目录>
node test/edit-smoke.js <地图工程目录>
```

## License

MIT
