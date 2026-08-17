#!/usr/bin/env node
'use strict';

/**
 * 蛋仔 EUI 工具箱 (Eggy EUI CLI) —— 单一入口
 *
 * 命令行参数统一由 commander 解析（子命令 / 选项 / 帮助 / 版本），
 * 各子命令的选项与行为在 lib/cmd-*.js 的 buildCommand() 中定义，本文件只负责组装与分发。
 *
 * 用法：
 *   eui export <草稿名称|地图目录|地图id> [选项]
 *   eui edit   <草稿名称|地图目录|地图id> [选项]
 *   eui list                             列出所有地图草稿
 *   eui types                            打印控件类型对照表
 *   eui config [set|get|unset|path]     管理配置（默认导出路径等）
 *   eui -h / --help                      本帮助
 *   eui -v / --version                   版本号
 *   兼容：eui <草稿名称|目录|id> [选项]  ==  eui export ...
 */

const { Command } = require('commander');
const cmdExport = require('../lib/cmd-export');
const cmdEdit = require('../lib/cmd-edit');
const cmdConfig = require('../lib/cmd-config');
const { c } = require('../lib/ansi');
const pkg = require('../package.json');

const VERSION = pkg.version;

/** 已知子命令名，用于旧用法兼容判定 */
const KNOWN_COMMANDS = new Set([
  'export', 'edit', 'list', 'types', 'config', 'help', 'version',
]);

/** 根程序帮助尾部（示例） */
function topHelpTail() {
  return [
    '',
    `${c.cyanBold('示例')}`,
    `  eui export                          # 交互式选择草稿并导出`,
    `  eui export 我的地图                 # 按草稿名称导出`,
    `  eui config set export-dir D:/out    # 设置默认导出目录`,
    `  eui edit 我的地图 --dump            # 查看字段`,
    `  eui edit 我的地图 --set "nodes[0].size[0]=100" --in-place`,
    `  eui list                            # 列出全部草稿`,
    '',
  ].join('\n');
}

/** 组装 commander 根程序（含全部子命令） */
function buildProgram() {
  const program = new Command();
  program
    .name('eui')
    .description('蛋仔派对 PC 编辑器 · EUI 工具箱\n从 eui.mm 导出 AI 可读 UI 数据包，或直接修改 eui.mm 字段。')
    .version(VERSION, '-v, --version', '显示版本号')
    .showHelpAfterError()
    .addHelpText('after', topHelpTail())
    .action(() => {
      // 无子命令时（裸 eui）打印帮助并正常退出（与旧版行为一致）
      program.outputHelp();
      process.exitCode = 0;
    });

  program.addCommand(cmdExport.buildCommand());
  program.addCommand(cmdEdit.buildCommand());
  program.addCommand(cmdConfig.buildCommand());

  // list / types：复用 export 逻辑
  program.addCommand(
    new Command('list')
      .description('列出所有地图草稿')
      .option('-r, --root <path>', '手动指定编辑器根目录')
      .option('--no-color', '禁用 ANSI 颜色')
      .action(async (opts) => {
        process.exitCode = (await cmdExport.run({ ...opts, list: true })) || 0;
      })
  );
  program.addCommand(
    new Command('types')
      .description('打印控件类型对照表')
      .option('--no-color', '禁用 ANSI 颜色')
      .action(async (opts) => {
        process.exitCode = (await cmdExport.run({ ...opts, listTypes: true })) || 0;
      })
  );
  // version 子命令（兼容旧用法 `eui version`）
  program.addCommand(
    new Command('version')
      .description('显示版本号')
      .action(() => {
        console.log(VERSION);
        process.exitCode = 0;
      })
  );

  return program;
}

async function main() {
  const program = buildProgram();
  // 把 commander 的 process.exit 改成抛 CommanderError，由下方统一收尾（含 zstd 句柄延迟退出）
  program.exitOverride();

  let argv = process.argv.slice(2);

  // 顶层 --no-color（子命令前）；子命令自身的 --no-color 由 commander 各自处理
  if (argv[0] === '--no-color') {
    process.env.NO_COLOR = '1';
    argv = argv.slice(1);
  }

  // 兼容旧用法：eui <草稿名称|目录|id> == eui export <草稿名称|目录|id>
  const first = argv.find((a) => !a.startsWith('-'));
  if (first && !KNOWN_COMMANDS.has(first)) {
    argv = ['export', ...argv];
  }

  try {
    await program.parseAsync(argv, { from: 'user' });
  } catch (e) {
    if (e && e.code && e.code.startsWith('commander.')) {
      // 帮助/版本/参数错误：commander 已打印输出，这里只取退出码
      process.exitCode = typeof e.exitCode === 'number' ? e.exitCode : 1;
    } else {
      // 子命令动作内未捕获的异常
      console.error(c.redBold('✖ ') + c.red(e && e.message ? e.message : String(e)));
      process.exitCode = 1;
    }
  }
}

main()
  .then(() => {
    // zstd-codec(wasm/asm) 会留下在关闭中的 uv async 句柄，立即 process.exit
    // 会触发 libuv 断言崩溃(0xC0000409)。稍等一拍让句柄闭合再退出。
    setTimeout(() => process.exit(process.exitCode), 60);
  })
  .catch((e) => {
    console.error(c.redBold('✖ ') + c.red(e && e.message ? e.message : String(e)));
    setTimeout(() => process.exit(1), 60);
  });
