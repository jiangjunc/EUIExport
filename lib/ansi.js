'use strict';

/**
 * 彩色输出与边框 —— 基于成熟库 chalk + boxen（不再手搓 ANSI / 手画框）。
 * - 颜色：chalk 自动检测 TTY / NO_COLOR / FORCE_COLOR
 * - 边框：boxen（默认圆角 ╭─╮ 风格）
 */

const chalk = require('chalk');
const boxen = require('boxen');

/** 运行时刷新颜色级别（cmd 在 run() 里设置 --no-color 后调用；FORCE_COLOR 优先级高于 NO_COLOR） */
function refresh() {
  const fc = process.env.FORCE_COLOR;
  if (fc === '0') {
    chalk.level = 0;
  } else if (fc) {
    chalk.level = ['1', '2', '3'].includes(fc) ? Number(fc) : 1;
  } else if (process.env.NO_COLOR) {
    chalk.level = 0;
  } else {
    chalk.level = process.stdout.isTTY ? 1 : 0;
  }
}
refresh();

const c = {
  bold: (s) => chalk.bold(s),
  dim: (s) => chalk.dim(s),
  underline: (s) => chalk.underline(s),
  red: (s) => chalk.red(s),
  green: (s) => chalk.green(s),
  yellow: (s) => chalk.yellow(s),
  blue: (s) => chalk.blue(s),
  magenta: (s) => chalk.magenta(s),
  cyan: (s) => chalk.cyan(s),
  cyanBold: (s) => chalk.cyan.bold(s),
  greenBold: (s) => chalk.green.bold(s),
  yellowBold: (s) => chalk.yellow.bold(s),
  redBold: (s) => chalk.red.bold(s),
  magentaBold: (s) => chalk.magenta.bold(s),
};

/**
 * 画一个带标题的框（boxen 圆角风格）。
 * @param {string} title 标题（可选）
 * @param {string[]} innerLines 内容行
 * @param {{borderStyle?: string, borderColor?: string, padding?: number|{top?:number,bottom?:number,left?:number,right?:number}}} [opts]
 */
function box(title, innerLines, opts = {}) {
  const content = title ? `${chalk.bold(title)}\n${innerLines.join('\n')}` : innerLines.join('\n');
  return boxen(content, {
    padding: opts.padding !== undefined ? opts.padding : { top: 0, bottom: 0, left: 2, right: 2 },
    borderStyle: opts.borderStyle || 'round',
    borderColor: opts.borderColor || 'cyan',
    float: 'left',
  });
}

/** 当前是否输出颜色（供需要预判的调用方使用；随 refresh() 更新） */
function useColor() {
  return chalk.level > 0;
}

module.exports = { c, box, refresh, useColor };
