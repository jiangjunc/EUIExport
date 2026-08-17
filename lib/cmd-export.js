'use strict';

/**
 * 蛋仔 EUI 导出命令 (eui export)
 *
 * 从蛋仔派对地图工程中解析 eui.mm，导出「AI 可读」的 UI 树 JSON。
 * 存储链路：eui.mm -> Zstandard 解压 -> MessagePack 解码 -> 可读化节点树
 *
 * 命令行参数统一由 commander 解析（见 buildCommand()）：
 *  - 支持按「地图草稿名称」获取地图
 *  - 无参数启动时交互式列出草稿供选择
 *  - --list 打印草稿名称列表
 *  - 草稿发现不依赖当前工作目录：自动从工具安装位置回溯 + 读取
 *    ~/.eggitor/cli/editor_config.json 定位编辑器根目录（任意 cwd 均可用）
 *  - -r/--root 手动指定编辑器根目录
 */

const fs = require('fs');
const path = require('path');
const { Command } = require('commander');
const { c, box } = require('./ansi');
const { decodeAll } = require('./msgpack');
const { ZSTDDecoder } = require('./zstd.cjs');
const { TYPE_MAP, exportScene, countNodes, treePreview, treeMarkdown, buildNodeIndex } = require('./eui');
const { buildReadme } = require('./readme');
const maps = require('./maps');
const config = require('./config');

const VERSION = '1.2.0';
const TOOL = '蛋仔 EUI 导出器';

/* ------------------------------------------------------------------ *
 *  CLI 命令定义（commander）
 * ------------------------------------------------------------------ */
function helpTail() {
  return [
    '',
    `${c.cyanBold('示例')}`,
    `  eui export                              # 交互式选择地图草稿`,
    `  eui export 我的地图                      # 按草稿名称导出`,
    `  eui export <base64id> -c                # 按 base64 地图 id + 压缩 JSON`,
    `  eui export "Documents/etc/editor_maps/<base64id>__pc"`,
    '',
    `${c.cyanBold('输出说明')}`,
    `  输出目录为“<草稿名>/”，内含三个文件：`,
    `  · UIData.json  完整节点数据：summary 统计 + node_index 索引 + scene 节点树`,
    `  · UITree.md    完整节点树状图索引（文本结构树，每行带 id）`,
    `  · README.md    AI 阅读指南 + 全部字段含义字典`,
    '',
    `${c.dim('说明：eui.mm 内部为 Zstandard 压缩 + MessagePack 序列化；')}`,
    `${c.dim('草稿名称取自各工程 desc.mm 的 map_name 字段。')}`,
  ].join('\n');
}

/** 构建 commander 子命令：eui export */
function buildCommand() {
  const cmd = new Command('export')
    .description('把 EUI 数据还原成 AI 可读的 UI 数据包（UIData.json / UITree.md / README.md）')
    .argument('[map]', '地图草稿名称 / 工程目录 / base64 地图 id（缺省则交互选择）')
    .option('-l, --list', '列出所有地图草稿名称后退出')
    .option('-i, --interactive', '强制进入交互选择（脚本 / 管道也可用）')
    .option('-r, --root <path>', '手动指定编辑器根目录（自动探测失败时用）')
    .option('-o, --output <dir>', '输出目录（直接指定）；未指定时按优先级取：EGGY_EUI_OUTPUT > eui config export-dir > 当前目录')
    .option('-c, --compact', 'UIData.json 输出压缩 JSON（默认美化缩进 2 格）')
    .option('-t, --list-types', '打印控件类型对照表后退出')
    .option('-q, --quiet', '安静模式：只打印一行结果摘要')
    .option('--no-color', '禁用 ANSI 颜色')
    .showHelpAfterError()
    .addHelpText('after', helpTail())
    .action(async (map, opts) => {
      const code = await run({ ...opts, map });
      process.exitCode = code || 0;
    });
  return cmd;
}

function printTypes() {
  const lines = [`${c.bold('控件类型对照表 (type -> 含义)')}`, ''];
  for (const [code, info] of Object.entries(TYPE_MAP).sort((a, b) => Number(a[0]) - Number(b[0]))) {
    lines.push(`  ${c.green(String(code).padStart(6))}  ${info.name.padEnd(14)} ${info.zh}`);
  }
  lines.push('');
  lines.push(c.dim('说明：type 为 eui.mm 中节点存贮的数字编码；cc_type 为编辑器内部控件类型字符串。'));
  return lines.join('\n');
}

/* ------------------------------------------------------------------ *
 *  地图目录 / 名称解析
 * ------------------------------------------------------------------ */
function listProjects(mapRoot) {
  return maps.listMapProjects(mapRoot);
}

function pickProject(root, projects, token) {
  if (projects.length === 1) return { dir: path.join(root, projects[0]), folder: projects[0] };
  const hit = maps.matchByPriority(projects, token);
  if (hit.length === 1) return { dir: path.join(root, hit[0]), folder: hit[0] };
  if (hit.length > 1) return { error: `“${token}” 匹配到多个工程，请指定唯一：\n${hit.map((d) => '  · ' + d).join('\n')}` };
  return {
    error: `目录下有 ${projects.length} 个地图工程，请指定唯一路径：\n${projects.map((d) => '  · ' + d).join('\n')}`,
  };
}

/** 判断 EUI 数据文件：FS 用 eui.mm，SE 用 euidata.mm */
function findEuiFile(dir) {
  if (fs.existsSync(path.join(dir, 'eui.mm'))) return 'eui.mm';
  if (fs.existsSync(path.join(dir, 'euidata.mm'))) return 'euidata.mm';
  return null;
}

/** 路径 / base64 id 解析（同步部分） */
function resolveMapDir(input, cwd) {
  const p = path.resolve(cwd, input);

  // 1) 直接指向含 EUI 数据文件（eui.mm / euidata.mm）的地图工程目录
  if (fs.existsSync(p)) {
    const st = fs.statSync(p);
    if (st.isDirectory()) {
      if (findEuiFile(p)) return { dir: p, folder: path.basename(p) };
      const projects = listProjects(p);
      if (projects.length) return pickProject(p, projects, path.basename(p));
      return { error: `目录中未找到 eui.mm / euidata.mm：${p}` };
    }
    if (st.isFile() && /\.gmp$/i.test(p)) {
      return { error: `${path.basename(p)} 是打包后的 .gmp 文件。\n请指向编辑中的地图工程目录（含 eui.mm / euidata.mm），如 Documents/etc/editor_maps/<id>__pc/ 或 Documents/etc/joint_construction_gmps/<id>/` };
    }
    return { error: `无法识别的路径：${p}` };
  }

  // 2) 按 base64 地图 id / 路径片段搜索全部地图工程根目录
  for (const root of maps.findMapRoots()) {
    const projects = listProjects(root);
    const hit = maps.matchByPriority(projects, input);
    if (hit.length === 1) return { dir: path.join(root, hit[0]), folder: hit[0] };
    if (hit.length > 1) return { error: `“${input}” 匹配到多个工程，请指定唯一：\n${hit.map((d) => '  · ' + d).join('\n')}` };
  }
  return { notFound: true };
}

/** 综合解析：路径/base64 失败后，再尝试按草稿名称匹配 */
async function resolveInput(input, cwd) {
  if (!input) return { interactive: true };
  const r = resolveMapDir(input, cwd);
  if (r.dir || r.error) return r;

  // 按草稿名称匹配
  const { drafts } = await maps.collectDrafts();
  if (drafts.length) {
    const hit = maps.matchByName(drafts, input);
    if (hit.length === 1) return { dir: hit[0].dir, folder: hit[0].folder, name: hit[0].name };
    if (hit.length > 1) {
      return { error: `草稿名称“${input}”匹配到多个，请更具体：\n${formatDraftList(hit)}` };
    }
  }
  return {
    error: `找不到地图“${input}”。\n可输入：草稿名称（如“我的地图”）/ 工程目录 / base64 地图 id，或用 -h 查看帮助。`,
  };
}

/* ------------------------------------------------------------------ *
 *  草稿列表展示
 * ------------------------------------------------------------------ */
function formatDraftList(drafts) {
  return drafts
    .map((d, i) => {
      const id = maps.parseFolderId(d.folder);
      const idPart = id ? c.dim('  id: ' + id.rawHex) : '';
      return `  ${String(i + 1).padStart(2)}.  ${c.bold(d.name)}  ${c.dim('(' + d.folder + ')')}${idPart}`;
    })
    .join('\n');
}

function printDraftList(drafts) {
  if (!drafts.length) {
    console.log(c.yellow('未找到任何地图草稿（editor_maps 下没有 “*__pc” 工程目录）。'));
    return;
  }
  console.log(`共 ${drafts.length} 个地图草稿：\n`);
  console.log(formatDraftList(drafts));
}

/* ------------------------------------------------------------------ *
 *  地图 id 解码 / 目录名清理
 * ------------------------------------------------------------------ */
function decodeMapId(folder) {
  return maps.parseFolderId(folder) || { base64: folder, hex: null, rawHex: null };
}

/** 把草稿名转成合法文件夹名 */
function sanitizeName(s) {
  const clean = String(s || '').trim().replace(/[\\/:*?"<>|\u0000-\u001f]/g, '_').replace(/[. ]+$/g, '').trim();
  return clean || 'map';
}

/* ------------------------------------------------------------------ *
 *  主流程（opts = commander 解析后的选项对象）
 * ------------------------------------------------------------------ */
async function run(opts = {}) {
  if (opts.color === false) process.env.NO_COLOR = '1';
  const map = opts.map || null;
  const cwd = process.cwd();

  if (opts.listTypes) {
    console.log(printTypes());
    return 0;
  }
  if (opts.root) maps.setEditorRoot(opts.root);

  // --list：只列草稿名称
  if (opts.list) {
    const { drafts } = await maps.collectDrafts();
    printDraftList(drafts);
    return 0;
  }

  if (!opts.quiet) {
    console.log(box(`${TOOL} v${VERSION}`, [
      c.dim('Eggy Party PC Editor · EUI -> AI-readable JSON'),
    ]));
    console.log('');
  }

  // 解析输入
  let resolved;
  if (!map) {
    // 无参数 -> 交互式选择草稿
    const { drafts } = await maps.collectDrafts();
    if (!drafts.length) {
      console.error(c.redBold('✖ ') + c.red('未找到任何地图草稿。\n请指定草稿名称 / 工程目录 / 地图 id，或用 -h 查看帮助。'));
      console.error(c.dim('已探测的编辑器根目录：\n' + maps.findEditorRoots().map((r) => '  · ' + r).join('\n')));
      return 1;
    }
    if (drafts.length === 1) {
      resolved = { dir: drafts[0].dir, folder: drafts[0].folder, name: drafts[0].name };
    } else {
      if (!opts.quiet) {
        console.log(c.bold('发现以下地图草稿：\n'));
        console.log(formatDraftList(drafts));
        console.log(c.dim('   0.  退出'));
        console.log('');
      }
      // 列表已打印，仅提示输入（与上方列表合并为一次输出）
      const picked = await maps.promptSelectDraft(drafts, opts.interactive, { showList: false });
      if (!picked) {
        if (!opts.quiet) {
          if (!opts.interactive && !process.stdin.isTTY) {
            console.log(c.yellow('\n非交互环境：请指定草稿名称 / 工程目录 / 地图 id（或用 -i 强制交互）。'));
          } else {
            console.log(c.yellow('\n已取消。'));
          }
        }
        return 1;
      }
      resolved = { dir: picked.dir, folder: picked.folder, name: picked.name };
    }
  } else {
    resolved = await resolveInput(map, cwd);
  }

  if (resolved.error) {
    console.error(c.redBold('✖ ') + c.red(resolved.error));
    return 1;
  }

  const mapDir = resolved.dir;
  const folderName = resolved.folder;
  const mapId = decodeMapId(folderName);
  // 草稿名称（解析时未带的，补充读取）
  let mapName = resolved.name || null;
  if (!mapName) mapName = await maps.readMapName(mapDir, maps.readVscodeNames());

  if (!opts.quiet) {
    console.log(c.bold('地图草稿') + `  ${c.cyan(mapName || '(未命名)')}`);
    console.log(c.bold('地图工程') + `  ${c.cyan(mapDir)}`);
    if (mapId.hex) console.log(c.bold('地图 ID  ') + `  base64: ${c.dim(mapId.base64)}   hex: ${c.dim(mapId.hex)}`);
    console.log('');
  }

  // 读取 EUI 数据文件（FS: eui.mm / SE: euidata.mm）
  const euiName = findEuiFile(mapDir);
  if (!euiName) {
    console.error(c.redBold('✖ ') + c.red(`未找到 ${path.join(mapDir, 'eui.mm')} 或 euidata.mm`));
    return 1;
  }
  const euiPath = path.join(mapDir, euiName);
  const compressed = fs.readFileSync(euiPath);

  // 解压 + 解码
  let raw, root;
  try {
    const dec = new ZSTDDecoder();
    await dec.init();
    raw = Buffer.from(dec.decode(new Uint8Array(compressed)));
  } catch (e) {
    console.error(c.redBold('✖ ') + c.red(`Zstandard 解压失败（可能不是合法的 eui.mm）：${e.message}`));
    return 1;
  }
  try {
    root = decodeAll(raw);
  } catch (e) {
    console.error(c.redBold('✖ ') + c.red(`MessagePack 解码失败：${e.message}`));
    return 1;
  }

  // 导出场景（含空值/结构异常守卫，报错给出可操作提示）
  let scene, stats;
  try {
    scene = exportScene(root);
    stats = countNodes(scene);
    stats.joystick_count = scene.joystick_count || 0;
  } catch (e) {
    console.error(
      c.redBold('✖ ') +
        c.red(
          `EUI 数据解析失败：${e.message}\n` +
            `  可能原因：该地图还没有可导出的 UI（新建地图未放任何控件）；或 eui.mm / euidata.mm 数据为空/损坏。\n` +
            `  建议：打开该地图放一个控件再保存，或确认选中的不是打包后的 .gmp 文件。`
        )
    );
    return 1;
  }

  // 输出目录，优先级：-o 参数 > EGGY_EUI_OUTPUT 环境变量 > 配置文件 exportDir > 当前目录
  const outDirName = sanitizeName(mapName || folderName);
  const cfgDir = config.get('exportDir');
  const envDir = process.env.EGGY_EUI_OUTPUT;
  let outDirBase = null; // 非空 = 用了「基础目录 + 草稿名」模式（env / config）
  let outDir;
  if (opts.output) {
    outDir = path.resolve(cwd, opts.output);
  } else if (envDir) {
    outDirBase = path.resolve(cwd, envDir);
    outDir = path.join(outDirBase, outDirName);
  } else if (cfgDir) {
    outDirBase = cfgDir;
    outDir = path.join(cfgDir, outDirName);
  } else {
    outDir = path.resolve(cwd, outDirName);
  }

  // 组装 UIData.json（完整节点数据，不含工具介绍）
  const doc = {
    format: 'eggy-eui-export',
    schema_version: 2,
    map_name: mapName,
    map_id: mapId.hex ? { base64: mapId.base64, hex: mapId.hex } : null,
    source_file: euiPath,
    exported_at: new Date().toISOString(),
    summary: {
      node_count: stats.count,
      canvas_count: stats.canvasCount,
      max_depth: stats.maxDepth,
      joystick_count: stats.joystick_count,
      type_counts: stats.typeCounts,
    },
    node_index: buildNodeIndex(scene),
    scene: scene,
  };

  // 写三个文件
  fs.mkdirSync(outDir, { recursive: true });
  const dataPath = path.join(outDir, 'UIData.json');
  const treePath = path.join(outDir, 'UITree.md');
  const readmePath = path.join(outDir, 'README.md');

  fs.writeFileSync(dataPath, (opts.compact ? JSON.stringify(doc) : JSON.stringify(doc, null, 2)) + '\n', 'utf8');

  // UITree.md —— 完整树状图索引
  const treeLines = treeMarkdown(scene);
  const treeMd = [
    `# UITree · ${mapName || '(未命名)'}`,
    '',
    `> 节点总数 ${stats.count} ｜ 画布 ${stats.canvasCount} ｜ 最大层级深度 ${stats.maxDepth} ｜ 系统控件/预置面板 ${stats.joystick_count}`,
    '> 每行 = 一个节点：`「名称」 [类型 #id]` + 关键信息。缩进与连接线表示父子层级；id 可到 `UIData.json` 的 `node_index` 精确定位。',
    '',
    '```',
    ...treeLines,
    '```',
    '',
  ].join('\n');
  fs.writeFileSync(treePath, treeMd, 'utf8');

  // README.md —— AI 阅读指南 + 字段字典
  const readmeMd = buildReadme({
    mapName,
    folder: outDirName,
    sourceFile: euiPath,
    stats,
    hasNodeIndex: true,
  });
  fs.writeFileSync(readmePath, readmeMd, 'utf8');

  const fmt = (p) => `${(fs.statSync(p).size / 1024).toFixed(1)} KB`;

  if (opts.quiet) {
    console.log(`${outDir}\t${stats.count} nodes\t(${fmt(dataPath)} + ${fmt(treePath)} + ${fmt(readmePath)})`);
    return 0;
  }

  // 报告
  console.log(c.greenBold('✔ 解析完成'));
  const typeLine = Object.entries(stats.typeCounts)
    .map(([t, n]) => `${t}=${c.yellow(String(n))}`)
    .join('  ');
  console.log(`   节点总数：${c.bold(String(stats.count))}    画布：${c.bold(String(stats.canvasCount))}    最大深度：${c.bold(String(stats.maxDepth))}`);
  console.log(`   类型分布：${typeLine}`);
  console.log('');

  const preview = treePreview(scene, 20);
  console.log(box('UI 树预览（前 ' + preview.length + ' 行）', preview, {}));
  console.log('');

  console.log(c.bold('输出目录') + `  ${c.green(outDir)}`);
  if (outDirBase) {
    console.log(c.dim(`  （基础目录来源：${envDir ? '环境变量 EGGY_EUI_OUTPUT' : '配置文件 exportDir'} = ${outDirBase}）`));
  }
  console.log(`  ├─ ${c.green('UIData.json')}  ${c.dim(fmt(dataPath))}  完整节点数据（summary + node_index + scene 树）`);
  console.log(`  ├─ ${c.green('UITree.md')}    ${c.dim(fmt(treePath))}  完整节点树状图索引（${treeLines.length} 行）`);
  console.log(`  └─ ${c.green('README.md')}    ${c.dim(fmt(readmePath))}  AI 阅读指南 + 字段字典`);
  console.log('');
  console.log(c.dim('提示：AI 先读 README.md 了解阅读顺序，再读 UITree.md 浏览层级，最后用 UIData.json 查节点细节。'));
  return 0;
}

module.exports = { run, buildCommand, printTypes };
