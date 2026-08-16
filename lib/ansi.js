'use strict';

/** 极简 ANSI 彩色输出（零依赖；非 TTY 或 NO_COLOR 时自动关闭） */
const useColor = process.stdout.isTTY && !process.env.NO_COLOR;

function wrap(code, s) {
  return useColor ? `\x1b[${code}m${s}\x1b[0m` : s;
}

const c = {
  bold: (s) => wrap('1', s),
  dim: (s) => wrap('2', s),
  underline: (s) => wrap('4', s),
  red: (s) => wrap('31', s),
  green: (s) => wrap('32', s),
  yellow: (s) => wrap('33', s),
  blue: (s) => wrap('34', s),
  magenta: (s) => wrap('35', s),
  cyan: (s) => wrap('36', s),
  cyanBold: (s) => wrap('1;36', s),
  greenBold: (s) => wrap('1;32', s),
  yellowBold: (s) => wrap('1;33', s),
  redBold: (s) => wrap('1;31', s),
  magentaBold: (s) => wrap('1;35', s),
};

/** 画一个带标题的框 */
function box(title, innerLines, opts = {}) {
  const width = Math.max(30, ...innerLines.map((l) => l.length), (title ? title.length + 4 : 0));
  const pad = '─'.repeat(width);
  const out = [];
  out.push(c.cyanBold(`╭─${pad}─╮`));
  if (title) out.push(c.cyanBold(`│ `) + c.bold(title) + c.cyanBold(` ${' '.repeat(Math.max(0, width - title.length - 1))}│`));
  for (const l of innerLines) out.push(c.cyanBold('│ ') + l + ' '.repeat(Math.max(0, width - l.length)) + c.cyanBold(' │'));
  out.push(c.cyanBold(`╰─${pad}─╯`));
  return out.join('\n');
}

module.exports = { c, useColor, box };
