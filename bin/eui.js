#!/usr/bin/env node
'use strict';

/**
 * 蛋仔 EUI 工具箱 (Eggy EUI CLI) —— 单一入口
 *
 * 合并两个工具：
 *   eui export  蛋仔 EUI 导出器：eui.mm -> AI 可读的 UI 数据包（UIData.json / UITree.md / README.md）
 *   eui edit    蛋仔 EUI 字段编辑器：直接改 eui.mm 字段（官方 Lua API 改不了的属性）
 *
 * 用法：
 *   eui export <草稿名称|地图目录|地图id> [选项]
 *   eui edit   <草稿名称|地图目录|地图id> [选项]
 *   eui list                             列出所有地图草稿
 *   eui types                            打印控件类型对照表
 *   eui -h / --help                      本帮助
 *   eui -v / --version                   版本号
 *   兼容：eui <草稿名称|目录|id> [选项]  ==  eui export ...
 */

const cmdExport = require('../lib/cmd-export');
const cmdEdit = require('../lib/cmd-edit');
const cmdConfig = require('../lib/cmd-config');
const { c } = require('../lib/ansi');
const pkg = require('../package.json');

const VERSION = pkg.version;

function topHelp() {
  const lines = [
    '',
    `${c.bold('蛋仔派对 PC 编辑器 · EUI 工具箱')}  ${c.dim('v' + VERSION)}`,
    '',
    `${c.dim('从 eui.mm 导出 AI 可读 UI 数据包，或直接修改 eui.mm 字段。')}`,
    `${c.dim('零运行时依赖（内置 Zstandard + MessagePack）。')}`,
    '',
    `${c.cyanBold('用法')}`,
    `  eui export <草稿名称|地图目录|地图id> [选项]  导出 UI 数据包`,
    `  eui edit   <草稿名称|地图目录|地图id> [选项]  修改 eui.mm 字段`,
    `  eui list [选项]                              列出所有地图草稿`,
    `  eui types                                   打印控件类型对照表`,
    `  eui config [set|get|unset|path]             管理配置（默认导出路径等）`,
    `  eui -h | --help                             显示本帮助`,
    `  eui -v | --version                          显示版本号`,
    '',
    `${c.cyanBold('示例')}`,
    `  eui export                          # 交互式选择草稿并导出`,
    `  eui export 我的地图                 # 按草稿名称导出`,
    `  eui config set export-dir D:/out    # 设置默认导出目录`,
    `  eui edit 我的地图 --dump            # 查看字段`,
    `  eui edit 我的地图 --set "nodes[0].size[0]=100" --in-place`,
    `  eui list                            # 列出全部草稿`,
    '',
    `${c.cyanBold('子命令帮助')}`,
    `  eui export -h      # 导出器参数（-o 输出目录 / -c 压缩 / -t 类型表 ...）`,
    `  eui edit -h        # 编辑器参数（--set / --patch / --dump / --in-place ...）`,
    `  eui config -h      # 配置管理（export-dir 默认导出路径 ...）`,
    '',
  ];
  console.log(lines.join('\n'));
}

async function main() {
  let argv = process.argv.slice(2);

  // 顶层 --no-color 剥离
  if (argv[0] === '--no-color') {
    process.env.NO_COLOR = '1';
    argv = argv.slice(1);
  }

  if (argv.length === 0) { topHelp(); return 0; }

  const cmd = argv[0];
  const rest = argv.slice(1);

  switch (cmd) {
    case '-h': case '--help': case 'help':
      topHelp();
      return 0;
    case '-v': case '--version': case 'version':
      console.log(VERSION);
      return 0;
    case 'export': case 'e':
      return await cmdExport.run(rest);
    case 'edit': case 'ed':
      return await cmdEdit.run(rest);
    case 'list': case 'ls':
      return await cmdExport.run(['--list', ...rest]);
    case 'types': case 'list-types':
      return await cmdExport.run(['--list-types', ...rest]);
    case 'config': case 'cfg':
      return await cmdConfig.run(rest);
    default:
      // 兼容旧用法：首参不是子命令时，整体当作 export 处理
      if (cmd.startsWith('-')) { topHelp(); return 0; }
      return await cmdExport.run(argv);
  }
}

main()
  .then((code) => {
    process.exitCode = code || 0;
    // zstd-codec(wasm/asm) 会留下在关闭中的 uv async 句柄，立即 process.exit
    // 会触发 libuv 断言崩溃(0xC0000409)。稍等一拍让句柄闭合再退出。
    setTimeout(() => process.exit(process.exitCode), 60);
  })
  .catch((e) => {
    console.error(c.redBold('✖ ') + c.red(e && e.message ? e.message : String(e)));
    setTimeout(() => process.exit(1), 60);
  });
