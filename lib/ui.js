'use strict';

/**
 * 表格渲染 —— 基于成熟库 cli-table3（不手搓对齐/画线）。
 * 符号统一用 figures（自动按平台回退 ASCII）。
 */

const Table = require('cli-table3');
const figures = require('figures');
const { useColor } = require('./ansi');

// 非 TTY / --no-color 时不给表格上色，避免把 ANSI 污染进管道/日志
const colorOn = () => (useColor() ? ['cyan'] : []);
const colorBorder = () => (useColor() ? ['gray'] : []);

// 去掉行间分隔线，只保留表头分隔（更清爽）
const lightChars = {
  mid: '',
  'left-mid': '',
  'mid-mid': '',
  'right-mid': '',
};

function baseTable(head) {
  return new Table({
    head,
    chars: lightChars,
    style: {
      head: colorOn(),
      border: colorBorder(),
      'padding-left': 1,
      'padding-right': 1,
    },
    wordWrap: true,
  });
}

/** 草稿列表表格（eui list / eui export -l / 非交互回退列表） */
function draftTable(drafts) {
  // 惰性 require：避免 maps <-> ui 循环依赖
  const maps = require('./maps');
  const t = baseTable(['#', '草稿名称', '工程目录', '地图 id']);
  drafts.forEach((d, i) => {
    const id = maps.parseFolderId(d.folder);
    t.push([String(i + 1), d.name, d.folder, id ? id.rawHex : '']);
  });
  return t.toString();
}

/** 控件类型对照表（eui types / eui export -t） */
function typeTable(entries) {
  const t = baseTable(['type', '名称', '含义']);
  for (const [code, info] of entries) t.push([String(code), info.name, info.zh]);
  return t.toString();
}

/** 键值表：row = [键, 类型, 值]（eui edit --dump 用） */
function keyValueTable(rows, head = ['字段', '类型', '值']) {
  const t = baseTable(head);
  for (const row of rows) t.push(row);
  return t.toString();
}

module.exports = { draftTable, typeTable, keyValueTable, figures };
