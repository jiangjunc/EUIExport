'use strict';

/**
 * 地图草稿发现与选择
 * - 草稿 = 各地图工程根目录下的工程文件夹（FS：`<base64-id>__pc`；SE：`<base64-id>` 或 `<base64-id>__pc`）
 * - 地图工程根目录：editor_maps（FS 草稿）/ help_build_gmps（官方教学）/ joint_construction_gmps（共建，多为 SE）/ se_maps 等
 * - 草稿名称（按优先级）：desc.mm 的 map_name > .gmp projectID 对应 lua 工程 eggy.json 的
 *   projectName > vscode_projs.json 的 VS Code 工程名
 * - EUI 数据文件：FS 用 eui.mm，SE 用 euidata.mm（两者 schema 同构，导出/编辑通用）
 */

const fs = require('fs');
const os = require('os');
const path = require('path');
const readline = require('readline');
const { decodeAll } = require('./msgpack');
const config = require('./config');

let _zstd = null;
async function getZstd() {
  if (!_zstd) {
    const { ZSTDDecoder } = require('./zstd.cjs');
    _zstd = new ZSTDDecoder();
    await _zstd.init();
  }
  return _zstd;
}

let _forcedRoot = null;
/** 手动指定编辑器根目录（--root），优先级最高 */
function setEditorRoot(root) {
  _forcedRoot = root ? path.resolve(root) : null;
}

/**
 * 候选「编辑器根目录」集合（按优先级，不做安装位置向上扫描）：
 * 0. --root 命令行参数（最高优先级，`eui export/edit ... -r <path>`）
 * 1. 用户配置文件 editor-root（~/.euiexport.json，用 `eui config set editor-root <path>` 持久化）
 * 2. 当前工作目录
 * 3. ~/.eggitor/cli/editor_config.json 的 project_dir / client_exe 所在目录（编辑器官方配置）
 */
function findEditorRoots() {
  const roots = new Set();
  if (_forcedRoot) roots.add(_forcedRoot);
  const cfgRoot = config.get('editorRoot');
  if (cfgRoot) roots.add(path.resolve(cfgRoot));
  roots.add(process.cwd());
  try {
    const cfgPath = path.join(os.homedir(), '.eggitor', 'cli', 'editor_config.json');
    if (fs.existsSync(cfgPath)) {
      const cfg = JSON.parse(fs.readFileSync(cfgPath, 'utf8'));
      if (typeof cfg.project_dir === 'string' && cfg.project_dir) roots.add(cfg.project_dir);
      if (typeof cfg.client_exe === 'string' && cfg.client_exe) roots.add(path.dirname(cfg.client_exe));
    }
  } catch { /* ignore */ }
  return [...roots];
}

/** 地图工程根目录的可能相对路径（<编辑器根>/Documents/etc/<dir>，或 <编辑器根>/<dir>） */
const MAP_SUBDIRS = [
  'editor_maps',           // FS 草稿
  'help_build_gmps',       // 官方教学地图（含 SE）
  'joint_construction_gmps', // 共建地图（多为 SE）
  'se_maps',               // 预留 SE 草稿目录
];

/** 探测全部「地图工程根目录」（跨候选编辑器根目录 × 各子目录） */
function findMapRoots() {
  const out = [];
  for (const root of findEditorRoots()) {
    for (const sub of MAP_SUBDIRS) {
      for (const base of [path.join('Documents', 'etc', sub), path.join('etc', sub), sub]) {
        const p = path.resolve(root, base);
        try {
          if (fs.existsSync(p) && fs.statSync(p).isDirectory() && !out.includes(p)) out.push(p);
        } catch { /* ignore */ }
      }
    }
  }
  return out;
}

/** 兼容旧名：返回第一个 editor_maps 根目录（找不到时退回任意地图根目录） */
function findEditorMapsRoot() {
  const roots = findMapRoots();
  return roots.find((p) => /editor_maps[\\/]?$/.test(p)) || roots[0] || null;
}

/** 判断目录是否为地图工程（存在地图数据文件 desc.mm / header.map 即视为工程） */
function isMapProjectDir(dir) {
  try {
    if (!fs.statSync(dir).isDirectory()) return false;
    return fs.existsSync(path.join(dir, 'desc.mm')) || fs.existsSync(path.join(dir, 'header.map'));
  } catch { return false; }
}

/** 列出某地图工程根目录下的全部工程文件夹（支持 FS 的 __pc 与 SE 的无后缀目录） */
function listMapProjects(mapRoot) {
  if (!mapRoot) return [];
  return fs
    .readdirSync(mapRoot)
    .filter((d) => isMapProjectDir(path.join(mapRoot, d)))
    .sort();
}

/** 解析草稿目录名里的 base64 地图 UUID（支持 “X__pc” 与纯 base64 “X” 两种 SE/FS 命名） */
function parseFolderId(folder) {
  let b64 = null;
  const m = /^([A-Za-z0-9+/=]+)__pc$/.exec(folder);
  if (m) b64 = m[1];
  else if (/^[A-Za-z0-9+/=]{16,40}$/.test(folder)) b64 = folder; // 无后缀：纯 base64 id 目录（SE 常见）
  if (!b64) return null;
  let bytes;
  try { bytes = Buffer.from(b64, 'base64'); } catch { return null; }
  if (bytes.length !== 16) return null; // 地图 UUID 固定 16 字节
  return {
    base64: b64,
    hex: bytes.toString('hex').toUpperCase().match(/.{1,2}/g).join(' '),
    rawHex: bytes.toString('hex'),
  };
}

/** 从草稿目录的 .gmp 文件提取工程 ID（magic 'DZ' + 4 字节头 + 12 字节 projectID） */
function readProjectIdFromGmp(projectDir) {
  try {
    const gmp = fs.readdirSync(projectDir).find((f) => f.endsWith('.gmp'));
    if (!gmp) return null;
    const b = fs.readFileSync(path.join(projectDir, gmp));
    if (b.length < 18 || b[0] !== 0x44 || b[1] !== 0x5a) return null; // 'DZ' 魔数
    return b.slice(6, 18).toString('hex');
  } catch { return null; }
}

/** 读取 lua 工程下 eggy.json 的 projectName（UTF-8，容忍 BOM / 解析失败） */
function readEggyProjectName(luaRoot) {
  try {
    const p = path.join(luaRoot, 'eggy.json');
    if (!fs.existsSync(p)) return null;
    let raw = fs.readFileSync(p, 'utf8');
    if (raw.charCodeAt(0) === 0xfeff) raw = raw.slice(1); // 去 BOM
    const j = JSON.parse(raw);
    if (typeof j.projectName === 'string' && j.projectName.trim()) return j.projectName.trim();
  } catch { /* ignore */ }
  return null;
}

/**
 * 读取编辑器记录的「lua 工程索引」（来源 Documents/vscode_projs.json，跨候选根目录）：
 *   { [projectID]: { luaRoot, projectName, vscodeName } }
 * - luaRoot     用户地图草稿的 lua 工程目录存放路径（编辑器记录）
 * - projectName 该 lua 工程 eggy.json 的 projectName
 * - vscodeName  从 LuaSource_<名> 目录名解析的 VS Code 工程名（兜底）
 */
function readProjectIndex() {
  for (const root of findEditorRoots()) {
    for (const rel of ['Documents/vscode_projs.json', 'vscode_projs.json']) {
      const p = path.resolve(root, rel);
      if (!fs.existsSync(p)) continue;
      try {
        const j = JSON.parse(fs.readFileSync(p, 'utf8'));
        const out = {};
        for (const [id, info] of Object.entries(j)) {
          const luaRoot = info.lua_root || info.workspace || '';
          const m = /LuaSource_(.*?)\s*$/.exec(luaRoot);
          out[id] = {
            luaRoot,
            projectName: readEggyProjectName(luaRoot),
            vscodeName: m ? m[1] : null,
          };
        }
        return out;
      } catch { /* ignore */ }
    }
  }
  return {};
}

/**
 * 读取单个草稿的名称（按优先级）：
 *   1. 编辑器 mm 文件：desc.mm 的 map_name
 *   2. 草稿 .gmp 里 projectID 对应 lua 工程 eggy.json 的 projectName
 *   3. vscode_projs.json 的 VS Code 工程名（LuaSource_<名>）
 */
async function readMapName(projectDir, projectIndex) {
  // 1. 编辑器 mm 文件
  const descPath = path.join(projectDir, 'desc.mm');
  if (fs.existsSync(descPath)) {
    try {
      const zstd = await getZstd();
      const raw = Buffer.from(zstd.decode(new Uint8Array(fs.readFileSync(descPath))));
      const data = decodeAll(raw);
      if (typeof data.map_name === 'string' && data.map_name.trim()) return data.map_name.trim();
    } catch { /* fallthrough */ }
  }
  // 2/3. 通过 .gmp 里的 projectID 找到编辑器记录的 lua 工程
  const projId = readProjectIdFromGmp(projectDir);
  const entry = projId ? (projectIndex[projId] || null) : null;
  if (entry) {
    if (entry.projectName) return entry.projectName; // eggy.json
    if (entry.vscodeName) return entry.vscodeName;   // vscode_projs.json
  }
  return null;
}

/** 收集全部草稿（跨所有地图工程根目录；与 cwd 无关，自动探测编辑器根目录） */
async function collectDrafts() {
  const roots = findMapRoots();
  if (!roots.length) return { root: null, roots: [], drafts: [] };
  const vscodeNames = readProjectIndex();
  const drafts = [];
  const seen = new Set(); // 同一地图 UUID 可能存在于多个根目录（如 help_build 与 joint_construction），去重
  for (const root of roots) {
    for (const folder of listMapProjects(root)) {
      const dir = path.join(root, folder);
      const name = await readMapName(dir, vscodeNames);
      const id = parseFolderId(folder);
      const key = id && id.rawHex;
      if (key) {
        if (seen.has(key)) continue;
        seen.add(key);
      }
      drafts.push({ folder, dir, name: name || folder, root });
    }
  }
  return { root: roots[0], roots, drafts };
}

/**
 * 按优先级模糊匹配：精确 > 前缀 > 包含（不区分大小写）。
 * 高优先级层只要有命中就只返回该层，避免“精确唯一”被“包含多个”掩盖：
 *   - 唯一命中 -> 调用方直接采用
 *   - 多个命中 -> 调用方报“匹配到多个”（此时低优先级层不再参与）
 * @param {Array} items 候选数组
 * @param {string} token 匹配串
 * @param {Function} [keyOf] 取候选的匹配键（默认取元素本身）
 * @returns {Array} 命中的候选（按优先级只取最高命中层）
 */
function matchByPriority(items, token, keyOf) {
  if (!Array.isArray(items) || !items.length || !token) return [];
  const t = String(token).trim().toLowerCase();
  const key = (item) => String(keyOf ? keyOf(item) : item).toLowerCase();
  let hit = items.filter((d) => key(d) === t);
  if (hit.length) return hit;
  hit = items.filter((d) => key(d).startsWith(t));
  if (hit.length) return hit;
  return items.filter((d) => key(d).includes(t));
}

/** 按名称/片段匹配草稿（精确 > 前缀 > 包含，避免“a”被“aaa”挤掉而误报冲突） */
function matchByName(drafts, token) {
  return matchByPriority(drafts, token, (d) => d.name);
}

/**
 * 交互式选择：等待用户输入编号。
 * 仅当 stdin 为 TTY，或 force=true（-i/--interactive）时才会真正等待输入。
 * 默认自行打印列表；若调用方已打印过列表，可传 { showList: false } 只显示输入提示。
 * @param {Array} drafts
 * @param {boolean} force
 * @param {{showList?: boolean, header?: string}} [opts]
 */
function promptSelectDraft(drafts, force, opts = {}) {
  return new Promise((resolve) => {
    const isTTY = Boolean(process.stdin.isTTY) || force;
    if (!isTTY) {
      resolve(null);
      return;
    }
    const { showList = true, header = '请选择要导出的地图草稿：' } = opts;
    const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
    const show = () => {
      if (!showList) return;
      console.log('');
      console.log(`  ${header}`);
      drafts.forEach((d, i) => {
        console.log(`  ${String(i + 1).padStart(2)}.  ${d.name}   ${' '.repeat(4)}${d.folder}`);
      });
      console.log('   0.  退出');
      console.log('');
    };
    show();
    rl.question('  输入编号后回车 > ', (answer) => {
      rl.close();
      const n = parseInt(answer, 10);
      if (answer.trim() === '0' || Number.isNaN(n) || n < 1 || n > drafts.length) {
        resolve(null);
        return;
      }
      resolve(drafts[n - 1]);
    });
  });
}

module.exports = {
  setEditorRoot,
  findEditorRoots,
  findEditorMapsRoot,
  findMapRoots,
  listMapProjects,
  parseFolderId,
  readProjectIdFromGmp,
  readEggyProjectName,
  readProjectIndex,
  readMapName,
  collectDrafts,
  matchByPriority,
  matchByName,
  promptSelectDraft,
};
