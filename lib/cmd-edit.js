'use strict';

/**
 * 蛋仔 EUI 字段编辑命令 (eui edit)
 *
 * 用途：修改「官方 Lua API 无法修改」的 UI 属性。
 *  - Meta 中 readOnly=true 的属性（API 只读，不允许 node.Prop = v 或 Set）
 *  - 未进入 Meta 的私有字段 / 内部工厂字段
 * 原理：绕过 API 层，直接改 eui.mm 存档：
 *       eui.mm -> zstd 解压 -> msgpack 解码 -> 改字段 -> msgpack 重编码
 *               -> zstd 压缩 -> 写回 -> 编辑器重新加载生效
 * 注意：编辑后需让编辑器重载（editor-cli map reopen --force，或关图重开），
 *       编辑器不会自动监听 mm 文件（见项目 README）。
 */

const fs = require('fs');
const path = require('path');
const readline = require('readline');
const { c } = require('./ansi');
const { decodeAll, encode } = require('./msgpack');
const { ZSTDDecoder } = require('./zstd.cjs');
const zstdCodec = require('./zstd-codec/zstd-codec.js');
const maps = require('./maps');

const VERSION = '0.1.0';
const TOOL = '蛋仔 EUI 字段编辑器';

/* ------------------------------------------------------------------ *
 *  参数解析
 * ------------------------------------------------------------------ */
function parseArgs(argv) {
  const opts = { map: null, sets: [], patch: null, dump: false, list: false,
    inPlace: false, output: null, root: null, quiet: false, noColor: false, help: false, unknown: null };
  const pos = [];
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') opts.help = true;
    else if (a === '--set') opts.sets.push(argv[++i]);
    else if (a.startsWith('--set=')) opts.sets.push(a.slice('--set='.length));
    else if (a === '--patch') opts.patch = argv[++i];
    else if (a.startsWith('--patch=')) opts.patch = a.slice('--patch='.length);
    else if (a === '--dump') opts.dump = true;
    else if (a === '-l' || a === '--list') opts.list = true;
    else if (a === '--in-place') opts.inPlace = true;
    else if (a === '-q' || a === '--quiet') opts.quiet = true;
    else if (a === '--no-color') opts.noColor = true;
    else if (a === '-r' || a === '--root') opts.root = argv[++i];
    else if (a.startsWith('--root=')) opts.root = a.slice('--root='.length);
    else if (a === '-o' || a === '--output') opts.output = argv[++i];
    else if (a.startsWith('--output=')) opts.output = a.slice('--output='.length);
    else if (a.startsWith('-') && a.length > 1) { opts.help = true; opts.unknown = a; }
    else pos.push(a);
  }
  opts.map = pos[0];
  return opts;
}

function helpText() {
  return [
    '',
    `${c.bold('蛋仔派对 PC 编辑器 · EUI 字段编辑器')}`,
    '',
    `${c.dim('修改官方 Lua API 无法修改的 UI 属性（Meta readOnly / 私有字段）：')}`,
    `${c.dim('直接改 EUI 存档（FS 的 eui.mm / SE 的 euidata.mm，zstd+msgpack 解码 -> 改字段 -> 重编码回写），再让编辑器重载。')}`,
    '',
    `${c.cyanBold('用法')}`,
    `  eui edit <草稿名称|地图目录|地图id> [选项]`,
    '',
    `${c.cyanBold('选项')}`,
    `  --set <path>=<value>    设置/新增字段，可多次（例：nodes[标题].size[0]=100）`,
    `  --patch <file.json>     应用 JSON Patch 数组 [{op,path,value}]，op: replace|add|remove`,
    `  --dump                  打印场景顶层字段 + 节点字段样本（用于找路径）`,
    `  -l, --list              列出所有地图草稿`,
    `  --in-place              直接写回原 eui.mm / euidata.mm（自动备份 .bak）`,
    `  -o, --output <file>     输出到指定文件（默认: <地图目录>/eui.modified.mm）`,
    `  -r, --root <path>       手动指定编辑器根目录`,
    `  -q, --quiet  --no-color  -h, --help`,
    '',
    `${c.cyanBold('路径语法')}`,
    `  nodes[0].size[0]        数组用 [下标]（或 .下标）`,
    `  nodes[按钮名].name      节点可用「名称」或「id」选择`,
    `  extra.background_color  嵌套对象用 . 逐层进`,
    `  --set 值能按 JSON 解析就按 JSON（数字/布尔/数组/对象），否则按字符串`,
    '',
    `${c.cyanBold('生效方式（二选一）')}`,
    `  1. 关掉该地图 -> 用改后的 eui.mm 覆盖原文件 -> 重新打开地图`,
    `  2. 地图打开中：editor-cli map reopen --force（丢弃未保存的内存修改）`,
    '',
  ].join('\n');
}

/* ------------------------------------------------------------------ *
 *  地图解析（复用导出器的发现逻辑）
 * ------------------------------------------------------------------ */
function resolveMapDir(input, cwd) {
  if (!input) return { interactive: true };
  const hasEuiFile = (p) => fs.existsSync(path.join(p, 'eui.mm')) || fs.existsSync(path.join(p, 'euidata.mm'));
  const cands = [];
  if (path.isAbsolute(input)) cands.push(input);
  else cands.push(path.resolve(cwd, input));
  for (const p of cands) {
    try {
      if (fs.statSync(p).isDirectory() && hasEuiFile(p)) {
        return { dir: p, folder: path.basename(p) };
      }
    } catch { /* ignore */ }
  }
  // base64 片段 -> 目录（跨全部地图工程根目录，含 SE）
  for (const root of maps.findMapRoots()) {
    const cand = path.join(root, input);
    try {
      if (fs.statSync(cand).isDirectory() && hasEuiFile(cand)) {
        return { dir: cand, folder: path.basename(cand) };
      }
    } catch { /* ignore */ }
    const hit = maps.listMapProjects(root).filter((d) => d.toLowerCase().includes(String(input).toLowerCase()));
    if (hit.length === 1) return { dir: path.join(root, hit[0]), folder: hit[0] };
    if (hit.length > 1) return { error: `“${input}” 匹配到多个工程目录：\n${hit.map((d) => '  · ' + d).join('\n')}` };
  }
  return { notFound: true };
}

async function resolveInput(input, cwd) {
  if (!input) return { interactive: true };
  const r = resolveMapDir(input, cwd);
  if (r.dir || r.error) return r;
  const { drafts } = await maps.collectDrafts();
  if (drafts.length) {
    const hit = maps.matchByName(drafts, input);
    if (hit.length === 1) return { dir: hit[0].dir, folder: hit[0].folder, name: hit[0].name };
    if (hit.length > 1) return { error: `草稿名称“${input}”匹配到多个：\n${hit.map((d) => '  · ' + d.name).join('\n')}` };
  }
  return { error: `找不到地图“${input}”。可输入草稿名称 / 工程目录 / 地图 id，或用 -h 查看帮助。` };
}

async function pickDraft() {
  const { drafts } = await maps.collectDrafts();
  if (!drafts.length) return null;
  if (drafts.length === 1) return drafts[0];
  console.log(c.bold('请选择要编辑的地图草稿：\n'));
  drafts.forEach((d, i) => console.log(`  ${String(i + 1).padStart(2)}.  ${c.bold(d.name)}  ${c.dim('(' + d.folder + ')')}`));
  console.log('   0.  退出\n');
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
  const pick = await new Promise((resolve) => {
    rl.question('  输入编号后回车 > ', (ans) => {
      const n = parseInt(ans, 10);
      resolve(ans.trim() === '0' || Number.isNaN(n) || n < 1 || n > drafts.length ? null : drafts[n - 1]);
    });
  });
  rl.close();
  return pick;
}

/* ------------------------------------------------------------------ *
 *  路径解析
 * ------------------------------------------------------------------ */
/** 按 名称 或 id 在 nodes 里找下标 */
function findNodeIndex(nodes, key) {
  for (let i = 0; i < nodes.length; i++) {
    const nd = nodes[i];
    if (String(nd.name) === key || String(nd.id) === key) return i;
  }
  return -1;
}

/**
 * 解析路径串（基对象 = scene，即 root[0]）
 * 返回 { parent, key, get, set } 或抛错
 */
function resolvePath(scene, pathStr) {
  // tokenize: nodes[0].size[0]  /  nodes.0.size.0  /  nodes[按钮].extra.x
  const tokens = [];
  const re = /([^.[\]]+)|\[([^\]]+)\]/g;
  let m;
  while ((m = re.exec(pathStr)) !== null) {
    tokens.push({ key: m[1] !== undefined ? m[1] : m[2], kind: m[1] !== undefined ? 'dot' : 'bracket' });
  }
  if (!tokens.length) throw new Error(`路径为空: ${pathStr}`);

  let node = scene;
  for (let i = 0; i < tokens.length - 1; i++) {
    const t = tokens[i];
    if (Array.isArray(node)) {
      if (/^\d+$/.test(t.key)) { node = node[Number(t.key)]; continue; }
      // 节点数组：支持按 名称/id 选择
      const idx = findNodeIndex(node, t.key);
      if (idx < 0) throw new Error(`路径 ${pathStr}: 在数组里找不到“${t.key}”（可用名称或 id）`);
      node = node[idx];
      continue;
    }
    if (node && typeof node === 'object') {
      if (node[t.key] === undefined && i === tokens.length - 1) {
        throw new Error(`路径 ${pathStr}: “${t.key}” 不存在（可用 --dump 看字段）`);
      }
      node = node[t.key];
      continue;
    }
    throw new Error(`路径 ${pathStr}: 在 ${JSON.stringify(node)} 上无法继续`);
  }
  const last = tokens[tokens.length - 1];
  if (Array.isArray(node)) {
    let idx;
    if (/^\d+$/.test(last.key)) idx = Number(last.key);
    else {
      idx = findNodeIndex(node, last.key);
      if (idx < 0) throw new Error(`路径 ${pathStr}: 数组里找不到“${last.key}”`);
    }
    return {
      parent: node, key: idx,
      get: () => node[idx],
      set: (v) => { node[idx] = v; },
      exists: idx < node.length,
    };
  }
  if (node && typeof node === 'object') {
    return {
      parent: node, key: last.key,
      get: () => node[last.key],
      set: (v) => { node[last.key] = v; },
      exists: last.key in node,
    };
  }
  throw new Error(`路径 ${pathStr}: 无法定位`);
}

/** --set 值解析：能 JSON 就 JSON，否则字符串 */
function parseValue(raw) {
  const s = String(raw).trim();
  if (s === '') return '';
  try {
    if (s[0] === '[' || s[0] === '{' || s[0] === '"' || s === 'true' || s === 'false' || s === 'null'
      || /^-?\d+(\.\d+)?([eE][+-]?\d+)?$/.test(s)) {
      return JSON.parse(s);
    }
  } catch { /* 按字符串 */ }
  return s;
}

/* ------------------------------------------------------------------ *
 *  字段预览（--dump）
 * ------------------------------------------------------------------ */
function typeOf(v) {
  if (v === null) return 'null';
  if (Array.isArray(v)) return `array[${v.length}]`;
  if (typeof v === 'object') return `obj{${Object.keys(v).join(',')}}`;
  return typeof v;
}

function dumpScene(scene) {
  console.log(c.cyanBold('场景顶层字段：'));
  for (const k of Object.keys(scene)) {
    console.log(`  ${c.bold(k)}  ${c.dim('(' + typeOf(scene[k]) + ')')}`);
  }
  const nodes = scene.nodes;
  if (Array.isArray(nodes) && nodes.length) {
    console.log(c.cyanBold('\nnodes 样本（第 0 个）：'));
    for (const k of Object.keys(nodes[0])) {
      console.log(`  ${c.bold(k)}  ${c.dim('(' + typeOf(nodes[0][k]) + ')')}  =  ${JSON.stringify(nodes[0][k]).slice(0, 70)}`);
    }
    const extra = nodes[0].extra;
    if (extra && typeof extra === 'object') {
      console.log(c.cyanBold('\nnode.extra 字段：'));
      for (const k of Object.keys(extra)) {
        console.log(`  extra.${c.bold(k)}  ${c.dim('(' + typeOf(extra[k]) + ')')}  =  ${JSON.stringify(extra[k]).slice(0, 60)}`);
      }
    }
    console.log(c.dim(`\n节点总数: ${nodes.length}。用 nodes[名称或id].字段 定位节点。`));
  }
}

/* ------------------------------------------------------------------ *
 *  主流程
 * ------------------------------------------------------------------ */
async function run(argv) {
  const opts = parseArgs(argv);
  if (opts.noColor) process.env.NO_COLOR = '1';
  if (opts.root) maps.setEditorRoot(opts.root);

  if (opts.help || opts.unknown) {
    if (opts.unknown) console.error(c.yellow(`未知选项：${opts.unknown}\n`));
    console.log(helpText());
    return 0;
  }
  if (opts.list) {
    const { drafts } = await maps.collectDrafts();
    if (!drafts.length) { console.log(c.yellow('未找到任何地图草稿。')); return 1; }
    console.log(`共 ${drafts.length} 个地图草稿：\n`);
    drafts.forEach((d, i) => console.log(`  ${String(i + 1).padStart(2)}.  ${c.bold(d.name)}  ${c.dim('(' + d.folder + ')')}`));
    return 0;
  }
  if (!opts.dump && !opts.sets.length && !opts.patch) {
    console.error(c.yellow('未指定任何修改（--set / --patch）。用 --dump 看字段，-h 看帮助。'));
    return 1;
  }

  if (!opts.quiet) console.log(`${c.bold(TOOL)} ${c.dim('v' + VERSION)}\n`);

  // 定位地图
  let resolved;
  if (!opts.map) {
    const picked = await pickDraft();
    if (!picked) { console.error(c.red('未选择地图。')); return 1; }
    resolved = { dir: picked.dir, folder: picked.folder };
  } else {
    resolved = await resolveInput(opts.map, process.cwd());
  }
  if (resolved.error || !resolved.dir) {
    console.error(c.redBold('✖ ') + c.red(resolved.error || '找不到地图。'));
    return 1;
  }
  const mapDir = resolved.dir;
  const mmName = fs.existsSync(path.join(mapDir, 'eui.mm')) ? 'eui.mm' : 'euidata.mm';
  const mmPath = path.join(mapDir, mmName);
  if (!fs.existsSync(mmPath)) { console.error(c.red(`✖ 没有 eui.mm / euidata.mm：${mapDir}`)); return 1; }

  // 读取 & 解码
  const dec = new ZSTDDecoder();
  await dec.init();
  const raw = Buffer.from(dec.decode(new Uint8Array(fs.readFileSync(mmPath))));
  const root = decodeAll(raw);
  const scene = root[0];
  if (!scene || typeof scene !== 'object') { console.error(c.red('✖ eui.mm 结构异常（root[0] 不是对象）。')); return 1; }

  if (opts.dump) { dumpScene(scene); return 0; }

  // 应用修改
  const applied = [];
  for (const set of opts.sets) {
    const eq = set.indexOf('=');
    if (eq <= 0) { console.error(c.yellow(`跳过无效 --set（缺 =）：${set}`)); continue; }
    const p = set.slice(0, eq).trim();
    const v = parseValue(set.slice(eq + 1));
    const r = resolvePath(scene, p);
    r.set(v);
    applied.push({ op: 'replace', path: p, value: v, existed: r.exists });
  }
  if (opts.patch) {
    let ops;
    try {
      const txt = fs.readFileSync(opts.patch, 'utf8').replace(/^\uFEFF/, ''); // 剥 BOM
      ops = JSON.parse(txt);
    }
    catch (e) { console.error(c.red(`✖ 无法读取 --patch 文件：${e.message}`)); return 1; }
    if (!Array.isArray(ops)) { console.error(c.red('✖ --patch 文件必须是 JSON 数组 [{op,path,value}]')); return 1; }
    for (const op of ops) {
      if (op.op === 'remove') {
        const r = resolvePath(scene, op.path);
        if (!r.exists) { console.error(c.yellow(`跳过 remove（不存在）：${op.path}`)); continue; }
        if (Array.isArray(r.parent)) r.parent.splice(r.key, 1);
        else delete r.parent[r.key];
        applied.push({ op: 'remove', path: op.path });
      } else {
        const r = resolvePath(scene, op.path);
        r.set(op.value);
        applied.push({ op: op.op || 'replace', path: op.path, value: op.value, existed: r.exists });
      }
    }
  }

  // 重编码 + 压缩
  const encoded = encode(root);
  const outBytes = await new Promise((resolve, reject) => {
    zstdCodec.run((codec) => {
      try {
        const simple = new codec.Simple();
        resolve(Buffer.from(simple.compress(new Uint8Array(encoded))));
      } catch (e) { reject(e); }
    });
  });

  // 写回
  let outPath;
  if (opts.inPlace) {
    const bak = mmPath + '.bak';
    fs.copyFileSync(mmPath, bak);
    fs.writeFileSync(mmPath, outBytes);
    outPath = mmPath;
    console.log(c.dim(`  已备份原文件 -> ${path.basename(bak)}`));
  } else {
    outPath = opts.output ? path.resolve(opts.output) : path.join(mapDir, 'eui.modified.mm');
    fs.writeFileSync(outPath, outBytes);
  }

  // 汇报
  console.log(c.greenBold('✔ 修改已写入：') + c.bold(outPath));
  console.log(c.dim(`  原 eui.mm: ${fs.statSync(mmPath).size} B  ->  新文件: ${outBytes.length} B`));
  for (const a of applied) {
    console.log(`  ${a.op}  ${c.bold(a.path)}  =  ${JSON.stringify(a.value)}${a.existed === false ? c.yellow('  (新增)') : ''}`);
  }
  const name = resolved.name || (await maps.readMapName(mapDir, maps.readVscodeNames())) || path.basename(mapDir);
  console.log('');
  console.log(c.cyan('生效方式：'));
  console.log(`  · 关掉该地图后，把新文件覆盖为原 eui.mm，再重新打开地图“${name}”；`);
  console.log(c.dim('  或：'));
  console.log(`  · 地图打开中执行  editor-cli map reopen --force  （会丢弃未保存的内存修改）`);
  return 0;
}

module.exports = { run, parseArgs, helpText };
