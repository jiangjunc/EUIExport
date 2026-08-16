'use strict';

/**
 * eui config —— 用户配置管理
 *
 * 用法：
 *   eui config path              显示配置文件路径
 *   eui config get [key]         读取配置（无 key 打印全部）
 *   eui config set <key> <value> 设置配置
 *   eui config unset <key>       删除配置
 *
 * 支持的键：
 *   export-dir    默认导出目录（基础目录；每次导出创建 <exportDir>/<草稿名>/）
 *                 优先级：-o 参数 > EGGY_EUI_OUTPUT 环境变量 > 此配置 > 当前目录
 */

const path = require('path');
const { c } = require('./ansi');
const config = require('./config');

// 用户友好的键名 -> 配置内部键
const KEY_ALIAS = {
  'export-dir': 'exportDir',
  'export_dir': 'exportDir',
};

function helpText() {
  return [
    '',
    `${c.bold('蛋仔派对 PC 编辑器 · EUI 工具箱 · 配置管理')}`,
    '',
    `${c.cyanBold('用法')}`,
    `  eui config path                    显示配置文件路径`,
    `  eui config get [key]               读取配置（无 key 打印全部）`,
    `  eui config set <key> <value>       设置配置`,
    `  eui config unset <key>             删除配置`,
    '',
    `${c.cyanBold('支持的键')}`,
    `  export-dir   默认导出目录（基础目录；每次导出创建 <exportDir>/<草稿名>/）`,
    `               优先级：-o 参数 > EGGY_EUI_OUTPUT 环境变量 > 此配置 > 当前目录`,
    '',
    `${c.cyanBold('示例')}`,
    `  eui config set export-dir D:/EUI_OUTPUT`,
    `  eui config get export-dir`,
    `  eui config unset export-dir`,
    '',
  ].join('\n');
}

function resolveKey(key) {
  return KEY_ALIAS[key] || key;
}

async function run(argv) {
  const sub = argv[0];

  if (!sub || sub === '-h' || sub === '--help' || sub === 'help') {
    console.log(helpText());
    return 0;
  }

  if (sub === 'path') {
    console.log(config.configPath());
    return 0;
  }

  if (sub === 'get') {
    const key = argv[1];
    if (!key) {
      console.log(JSON.stringify(config.load(), null, 2));
      return 0;
    }
    const v = config.get(resolveKey(key));
    console.log(v === undefined ? '' : (typeof v === 'string' ? v : JSON.stringify(v)));
    return 0;
  }

  if (sub === 'set') {
    const key = argv[1];
    const value = argv[2];
    if (!key || value === undefined) {
      console.error(c.yellow('用法：eui config set <key> <value>'));
      return 1;
    }
    const internal = resolveKey(key);
    let v = value;
    // 导出路径：相对路径转绝对，便于从任意 cwd 使用
    if (internal === 'exportDir' && value && !path.isAbsolute(value)) {
      v = path.resolve(process.cwd(), value);
    }
    config.set(internal, v);
    console.log(c.greenBold('✔ ') + `${key} = ${c.bold(v)}`);
    console.log(c.dim('配置文件：' + config.configPath()));
    return 0;
  }

  if (sub === 'unset') {
    const key = argv[1];
    if (!key) {
      console.error(c.yellow('用法：eui config unset <key>'));
      return 1;
    }
    config.unset(resolveKey(key));
    console.log(c.greenBold('✔ ') + `已删除 ${key}`);
    console.log(c.dim('配置文件：' + config.configPath()));
    return 0;
  }

  console.error(c.yellow(`未知配置子命令：${sub}\n`));
  console.log(helpText());
  return 1;
}

module.exports = { run, helpText };
