'use strict';

/**
 * EUI 工具箱用户配置
 *
 * 配置文件：~/.eggy-eui-cli.json
 *          （可用环境变量 EGGY_EUI_CONFIG 覆盖配置文件路径，适合多环境/CI）
 *
 * 键说明：
 *   exportDir  默认导出目录（基础目录；每次导出创建 <exportDir>/<草稿名>/）
 *              优先级：-o 参数 > EGGY_EUI_OUTPUT 环境变量 > exportDir 配置 > 当前目录
 */

const fs = require('fs');
const os = require('os');
const path = require('path');

const DEFAULT_FILE = path.join(os.homedir(), '.eggy-eui-cli.json');

/** 配置文件路径（EGGY_EUI_CONFIG 可覆盖） */
function configPath() {
  return process.env.EGGY_EUI_CONFIG || DEFAULT_FILE;
}

/** 读取配置（文件不存在 / 损坏时返回空对象） */
function load() {
  try {
    const raw = fs.readFileSync(configPath(), 'utf8');
    const obj = JSON.parse(raw);
    return (obj && typeof obj === 'object' && !Array.isArray(obj)) ? obj : {};
  } catch {
    return {};
  }
}

/** 写回配置（自动建目录） */
function save(obj) {
  fs.mkdirSync(path.dirname(configPath()), { recursive: true });
  fs.writeFileSync(configPath(), JSON.stringify(obj, null, 2) + '\n', 'utf8');
}

function get(key) {
  return load()[key];
}

function set(key, value) {
  const obj = load();
  obj[key] = value;
  save(obj);
  return value;
}

function unset(key) {
  const obj = load();
  const had = Object.prototype.hasOwnProperty.call(obj, key);
  delete obj[key];
  save(obj);
  return had;
}

module.exports = { DEFAULT_FILE, configPath, load, save, get, set, unset };
