'use strict';

/**
 * eui config —— 用户配置管理（子命令由 commander 定义）
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
 *   editor-root   编辑器根目录（自动发现失败时手动指定）
 *                 优先级：-r/--root 参数 > 此配置 > ~/.eggitor/cli/editor_config.json > 当前目录
 */

const path = require('path');
const { Command } = require('commander');
const { c } = require('./ansi');
const config = require('./config');

// 用户友好的键名 -> 配置内部键
const KEY_ALIAS = {
  'export-dir': 'exportDir',
  'export_dir': 'exportDir',
  'editor-root': 'editorRoot',
  'editor_root': 'editorRoot',
};

function resolveKey(key) {
  return KEY_ALIAS[key] || key;
}

/** 构建 commander 子命令：eui config（含 path/get/set/unset 子命令） */
function buildCommand() {
  const cmd = new Command('config')
    .description('管理配置（默认导出路径等）')
    .showHelpAfterError()
    .action(() => {
      // 无子命令时（裸 eui config）打印帮助并正常退出（与旧版行为一致）
      cmd.outputHelp();
    });

  cmd.command('path')
    .description('显示配置文件路径')
    .action(() => {
      console.log(config.configPath());
    });

  cmd.command('get')
    .description('读取配置（无 key 打印全部）')
    .argument('[key]', '配置键')
    .action((key) => {
      if (!key) {
        console.log(JSON.stringify(config.load(), null, 2));
        return;
      }
      const v = config.get(resolveKey(key));
      console.log(v === undefined ? '' : (typeof v === 'string' ? v : JSON.stringify(v)));
    });

  cmd.command('set')
    .description('设置配置')
    .argument('<key>', '配置键')
    .argument('<value>', '配置值')
    .action((key, value) => {
      const internal = resolveKey(key);
      let v = value;
      // 路径类配置：相对路径转绝对，便于从任意 cwd 使用
      if ((internal === 'exportDir' || internal === 'editorRoot') && value && !path.isAbsolute(value)) {
        v = path.resolve(process.cwd(), value);
      }
      config.set(internal, v);
      console.log(c.greenBold('✔ ') + `${key} = ${c.bold(v)}`);
      console.log(c.dim('配置文件：' + config.configPath()));
    });

  cmd.command('unset')
    .description('删除配置')
    .argument('<key>', '配置键')
    .action((key) => {
      config.unset(resolveKey(key));
      console.log(c.greenBold('✔ ') + `已删除 ${key}`);
      console.log(c.dim('配置文件：' + config.configPath()));
    });

  return cmd;
}

module.exports = { buildCommand };
