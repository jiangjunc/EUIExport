<p align="center">
  <img src="cover.png" alt="EUIExport" width="100%">
</p>

<h1 align="center">EUIExport · 蛋仔派对 EUI 导出器</h1>

<p align="center">
  <a href="https://opensource.org/licenses/MIT"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
  <a href="https://github.com/jiangjunc/EUIExport"><img src="https://img.shields.io/badge/GitHub-jiangjunc%2FEUIExport-blue" alt="GitHub"></a>
  <a href="https://github.com/jiangjunc/EUIExport/releases"><img src="https://img.shields.io/badge/Release-2.0.0-green" alt="Release: 2.0.0"></a>
</p>

<p align="center">
  把蛋仔编辑器的 EUI 还原为 <b>AI 可读</b> 的数据包 · C11 实现 · 单文件可执行
  <br>
  支持 <b>FS（帧同步）</b> 与 <b>SE（世界版 / 状态同步）</b> 两类地图
  <br>
  <sub>2.0.0 为 C11 重写版，从 Release 下载 exe 即可；npm 上的 1.3.0（Node.js 实现）保持原样，不受影响</sub>
</p>

---

## 特性

蛋仔编辑器的 UI（EUI）存放在地图工程的 `eui.mm`（FS）/ `euidata.mm`（SE）里，内部是 **Zstandard 压缩 + MessagePack 序列化** 的二进制数据，非常不利于人类和 AI 阅读。本工具把它还原成可读数据。

| 子命令 | 作用 |
| --- | --- |
| `eui export` | 把 EUI 数据还原成 **AI 可读** 的 UI 数据包（JSON + 树状图 + 字段字典） |
| `eui list`   | 列出全部地图草稿 |
| `eui types`  | 打印控件类型对照表 |
| `eui config` | 管理用户配置（默认导出目录 / 编辑器根目录） |

- 同时支持 FS（`eui.mm`）与 SE（`euidata.mm`），自动识别
- **纯 C11 实现，无运行时依赖**：解压（Zstandard）、解码（MessagePack）、JSON 序列化全部内置
- 产物为**单文件 exe**（静态链接，约 208 KB），拷贝即用
- **双击即可运行**：自动进入交互选择，结束后等待按键，不会一闪而过
- **自动注册到用户 PATH**：任何一次运行都会把自身目录写入用户环境变量，之后可在任意目录直接 `eui`；移动 exe 后再次运行会自动替换旧路径
- 全屏交互式草稿选择（中文模糊筛选、CJK 宽度对齐的表格、真彩色降级兼容）
- 自动扫描 `editor_maps` / `help_build_gmps` / `joint_construction_gmps` / `se_maps`
- 同一地图多副本时**自动读最新数据**（避免导出到 `editor_maps` 等过期副本）
- 输出与旧版 Node.js 实现**逐字节一致**（`UIData.json` / `UITree.md` / `README.md` 三件产物，仅 `exported_at` 时间戳不同）

## 构建

只需要一个 **MinGW-w64 gcc**（MSYS2 的 `ucrt64` 或 `mingw64` 均可），无需 Node.js、CMake 或任何第三方库：

```bat
:: Windows（自动探测 C:\msys64\ucrt64\bin\gcc.exe）
build.bat
```

```bash
# MSYS2 / Git Bash
./build.sh
```

产物：`build\eui.exe`（单文件，静态链接，可直接分发）。

构建参数：`-std=c11 -Os -Wall -Wextra -static -s -ffunction-sections -fdata-sections -Wl,--gc-sections`。
`-Os` 与 `-O2` 产物逐字节相同，体积小约 18%。全部源码一次编译链接（约 30 个编译单元，秒级完成），不产生中间 `.o` 文件。

## 双击运行与全局命令

- **双击 `eui.exe`**：打开控制台并进入草稿选择，导出结束后显示「按任意键退出…」，窗口不会一闪而过。
- **自动注册 PATH**：程序每次启动都会把自身所在目录写入 `HKCU\Environment` 的 `Path`（仅用户级，不需要管理员权限），并广播 `WM_SETTINGCHANGE`。之后在任意目录可直接 `eui export`。
  - 幂等：已注册且路径未变时**不写注册表**。
  - 迁移自愈：把 exe 换到别的目录再运行，旧目录会被自动移除、新目录写入（记在 `EUIEXPORT_HOME` 里）。
  - 只增删自己那一项，`%USERPROFILE%` 之类的可展开变量与其它条目原样保留。
  - 关闭：设置环境变量 `EUI_NO_PATH=1`。
  - 已经打开的终端/编辑器不会感知新 PATH，重开即可（或注销/重启）。

## 快速开始

```bat
:: 交互式选择草稿并导出
eui export

:: 按草稿名称导出（支持：地图全名 / 前缀匹配 / 关键词匹配）
eui export 我的地图

:: 指向编辑器的地图本地数据目录，或用 base64 地图 id
:: 编辑器的地图本地数据目录: 一般在 ./Documents/etc/editor_maps/
:: base64 地图 id: 从 Lua 工程 eggy.json 里的 projectID 字段获取
eui export "./Documents/etc/editor_maps/<base64id>__pc"
eui export <base64id> -c

:: 列出全部草稿 / 查看类型对照表
eui list
eui types
```

## CLI

```
eui export [地图] [选项]      导出 UI 数据包（缺省交互选择草稿）
eui list [选项]               列出所有地图草稿
eui types                     打印控件类型对照表
eui config <子命令>           管理配置（导出目录 / 编辑器根目录）
eui version                   显示版本号
eui help                      显示帮助
```

`export` 选项：

| 选项 | 说明 |
| --- | --- |
| `-l, --list` | 列出草稿名称后退出 |
| `-i, --interactive` | 强制交互选择（管道 / 脚本环境也可用） |
| `-r, --root <路径>` | 手动指定编辑器根目录 |
| `-o, --output <目录>` | 直接指定输出目录 |
| `-c, --compact` | `UIData.json` 输出压缩 JSON（默认缩进 2 格） |
| `-t, --list-types` | 打印控件类型对照表后退出 |
| `-q, --quiet` | 安静模式：只打印一行结果摘要（适合脚本 / CI） |
| `--no-color` | 禁用 ANSI 颜色（也支持 `NO_COLOR` 环境变量） |

完整帮助见 `eui export -h` / `eui config -h` / `eui list -h`。

## 配置

### 默认导出路径

不想每次敲 `-o`？两种方式可持久设置默认导出路径，之后所有导出自动写入 `<基础目录>/<草稿名>/`：

```bat
:: 方式一：配置文件（推荐，写入 ~/.euiexport.json）
eui config set export-dir D:/EUI_OUTPUT
eui config get export-dir
eui config unset export-dir
eui config path

:: 方式二：环境变量（适合脚本 / CI）
set EGGY_EUI_OUTPUT=D:/EUI_OUTPUT
```

**导出目录优先级**：`--output 参数` > `EGGY_EUI_OUTPUT 环境变量` > `eui config export-dir` > 草稿对应的 lua 工程目录 > 桌面。

> 默认（未指定 `-o` / 环境变量 / 配置）时，导出到该草稿对应的 lua 工程目录（如 `LuaSource_记忆卡牌/记忆卡牌/`）；找不到 lua 工程时兜底到**桌面**（通过系统 API 取当前用户桌面路径，不写死盘符）。

### 编辑器根目录

工具靠「编辑器根目录」发现地图草稿（`editor_maps` 等都在编辑器安装目录下）。查找优先级：

1. `-r, --root <path>` 命令行参数
2. `eui config set editor-root <path>` 配置文件（持久化）
3. 当前工作目录
4. `~/.eggitor/cli/editor_config.json`（`project_dir` / `client_exe`）

```bat
:: 持久化指定编辑器根目录
eui config set editor-root D:/FeverApps/party_pc

:: 或本次临时指定
eui export -r D:/FeverApps/party_pc 我的地图
```

> 配置文件 `~/.euiexport.json` 属于用户级配置；也可用环境变量 `EGGY_EUI_CONFIG` 指定其他配置文件路径（多环境 / CI 场景）。

## 进阶

### 草稿名称来源

每个地图草稿的名称按优先级：

1. **编辑器 mm 文件**：工程 `desc.mm` 里的 `map_name`（权威）
2. **lua 工程 `eggy.json` 的 `projectName`**：从 `.gmp` 的 projectID 找到 `vscode_projs.json` 记录的 lua 工程
3. **`vscode_projs.json`**：VS Code 工程名（从 `LuaSource_<名>` 目录名提取）

### 导出到哪个 lua 工程

未显式指定输出目录时，按下面的顺序找草稿对应的 lua 工程，**只要有一档唯一命中就停**：

1. **`.gmp` 工程 id**：草稿目录里有 `.gmp`（构建过）时，读其中的工程 id，在 `Documents/vscode_projs.json` 里精确匹配 `lua_root`；
2. **工程绑定的草稿**：工程 `eggy.json` 的 `mapDir` 记录它当前打开的草稿目录，按目录 uuid 比对（兼容 `base64(hex)` 与 `base64(16 字符 id)` 两种历史命名）；
3. **工程端口**：草稿 `desc.mm` 的 `vscode_server_port` 就是它最近连过的 lua 工程端口，与工程 `eggy.json` 的 `serverPort` 比对。草稿被**复制、改名**后该字段仍保留，因此「多个草稿共享同一 lua 工程」「副本名与工程名对不上」都能对应上；
4. **复制来源**：草稿 `desc.mm` 的 `alias` 记录被复制草稿的 uuid，先直接查工程 id，再在扫描到的草稿里找同 uuid 的原始草稿，用它的结果（递归一层）；
5. **草稿名**：与工程 `eggy.json` 的 `projectName` 或 `LuaSource_<名>` 目录名比对，且只有唯一命中才认；
6. 都找不到 → 兜底导出到**当前用户桌面**（`SHGetFolderPathW`，安装到非系统盘或重定向桌面也能正确取到）。

### 扫描范围

自动遍历 FS 与 SE 的所有地图工程根目录（`editor_maps` / `help_build_gmps` / `joint_construction_gmps` / `se_maps`），并识别 FS（`eui.mm`）与 SE（`euidata.mm`）两种工程；同一地图存在于多个根目录时按地图 UUID 去重，**并保留保存时间最新的一份**（避免导出到 `editor_maps` 等目录里的过期副本，导致节点与编辑器实际 UI 对不上）。

### `export` 输出

输出是一个文件夹（默认以草稿名命名），包含三个文件：

| 文件 | 作用 |
| --- | --- |
| `UIData.json` | 完整节点数据：`summary` 统计 + `node_index` 快速索引 + `scene` 完整 UI 树 |
| `UITree.md`   | 完整节点树状图索引（每行 `「名称」 [类型 #id]` + 关键信息） |
| `README.md`   | AI 阅读指南 + 全部字段含义字典（建议 AI 第一个读它） |

`UIData.json` 顶层结构：

```json
{
  "format": "eggy-eui-export",
  "schema_version": 2,
  "map_name": "<地图草稿名>",
  "map_id": { "base64": "...", "hex": "..." },
  "source_file": "<eui.mm 绝对路径>",
  "exported_at": "<ISO 8601 时间>",
  "summary": {
    "node_count": 114,
    "canvas_count": 4,
    "max_depth": 5,
    "joystick_count": 28,
    "type_counts": { }
  },
  "node_index": {
    "<节点id>": { "name": "标题", "type": "Text", "path": "主界面 > 标题" }
  },
  "scene": {
    "type": "Scene",
    "version": "0.0.1",
    "nodes": [ "/* 根画布节点树 */" ]
  }
}
```

`scene.nodes` 里每个节点对象包含可读分组：`layout`（布局几何）、`visibility`（显隐规则）、`events`（触摸与显隐事件）、`behavior`（交互）、`text/image/panel/background`（外观，颜色已转 `#hex`）、`children`（子节点）、`other`（未归类原始字段）。

### 原理

```
EUI 数据（FS: eui.mm / SE: euidata.mm）
  └─ Zstandard 解压（vendor 内置，decompress-only）
       └─ MessagePack 解码（保留键序；越界整数降级为十进制字符串）
            └─ 节点可读化 ──→ UIData.json / UITree.md / README.md
```

数值与 JSON 的格式化严格复刻 ECMAScript `Number#toString` / `JSON.stringify` 的语义（最短往返数字串、对象键序、控制字符转义、非法 UTF-8 → U+FFFD），因此输出与旧实现逐字节等同。

## UI 编辑（已移除 · 不确定的猜想）

> ⚠️ **本节内容为不确定的猜想，未经验证，不属于当前版本能力范围。**

当前版本**只支持导出，不再支持 UI 编辑**。旧版的 `eui edit`（改字段后重新压缩写回 `eui.mm` / `euidata.mm`）已被整体移除：写回需要 MessagePack **编码器**与 Zstandard **压缩器**，而当前二进制只内置了解压/解码半边。

以下只是关于「编辑能力可能如何恢复」的**猜想**，不构成承诺，也没有经过验证：

- 猜测可以在导出模型上做字段级修改，再自行实现 MessagePack 编码与 Zstandard 压缩写回；
- 猜测写回后需要重载地图（关图重开，或编辑器 CLI `map reopen --force`）才能看到修改；
- 猜测官方 Lua API 无法修改的 Meta 只读 / 私有字段，可能可以通过直接改存档绕开——**这一点完全没有验证**；
- 上述路径可能因编辑器版本、数据布局变化而完全失效。

如果需要编辑能力，请使用旧版 Node.js 实现（git 历史中可找回），并自行承担数据损坏风险；务必先备份 `eui.mm` / `euidata.mm`。

## 环境要求

- Windows（控制台交互基于 Win32 Console API）
- 构建：MinGW-w64 gcc（MSYS2 `ucrt64` / `mingw64`）
- 运行：无依赖；蛋仔派对 PC 编辑器（Eggitor）安装目录（用于自动发现地图草稿）

## 项目结构

```
EUIExport/
├── src/
│   ├── main.c          # 入口（UTF-8 argv、双击检测、PATH 自注册）
│   ├── selfreg.c/.h    # 用户 PATH 自注册 / 迁移自愈 / 双击等待按键
│   ├── cmd.c/.h        # 子命令、参数解析、导出编排、控制台报告
│   ├── tui.c/.h        # 表格 / 边框 / 全屏模糊选择器
│   ├── tty.c/.h        # Win32 控制台（UTF-8、VT、raw 键盘）
│   ├── eui.c/.h        # EUI 节点可读化 / 统计 / 树索引 / 节点索引
│   ├── mdout.c/.h      # UITree.md 与输出包内 README.md
│   ├── maps.c/.h       # 地图草稿发现、命名与选择
│   ├── msgpack.c/.h    # MessagePack 解码（有序 map）
│   ├── zstdwrap.c/.h   # Zstandard 解压封装
│   ├── json.c/.h       # JSON DOM + JS 语义序列化
│   ├── utf8.c/.h       # UTF-8、CJK 宽度、base64
│   ├── fs.c/.h         # 文件系统与路径（Win32 宽字符）
│   ├── config.c/.h     # 用户配置（~/.euiexport.json）
│   └── util.c/.h       # Arena / 字符串构建器 / 小工具
├── vendor/zstd/        # Zstandard 1.5.6（仅解压子集，静态编译）
├── build.bat / build.sh
├── cover.png
├── README.md
└── LICENSE
```

## 测试

以真实地图工程为输入的回归方式：用旧版 Node.js 实现生成 golden（`UIData.json` / `UITree.md` / `README.md`），再与 C 版输出逐字节 diff，**仅忽略 `exported_at` 时间戳**。

覆盖 18 个真实草稿（FS + SE）：可导出的 14 个全部逐字节一致；另外 4 个草稿本身没有 `eui.mm` / `euidata.mm`，两版给出同样的「未找到」错误。

```bat
eui export -o out\fs "<地图工程目录>"
fc out\fs\UITree.md golden\fs\UITree.md
```

## License

[MIT](LICENSE)
