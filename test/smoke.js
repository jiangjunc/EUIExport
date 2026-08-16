'use strict';

/**
 * 冒烟测试：
 *  - 指定地图工程跑完整流水线并断言结果（含 UITree / node_index 新部件）
 *  - 草稿发现 / 名称解析
 * 用法：node test/smoke.js <地图工程目录或editor_maps根或base64 id>
 */

const fs = require('fs');
const path = require('path');
const { ZSTDDecoder } = require('../lib/zstd.cjs');
const { decodeAll } = require('../lib/msgpack');
const { exportScene, countNodes, treeMarkdown, buildNodeIndex } = require('../lib/eui');
const maps = require('../lib/maps');

async function loadScene(input) {
  const euiPath = path.join(path.resolve(process.cwd(), input), 'eui.mm');
  if (!fs.existsSync(euiPath)) throw new Error(`未找到 ${euiPath}`);
  const dec = new ZSTDDecoder();
  await dec.init();
  const raw = Buffer.from(dec.decode(new Uint8Array(fs.readFileSync(euiPath))));
  return exportScene(decodeAll(raw));
}

function countAll(nodes) {
  let n = 0;
  (function walk(list) { for (const x of list) { n++; if (x.children) walk(x.children); } })(nodes);
  return n;
}

async function main() {
  const input = process.argv[2];
  const cwd = process.cwd();
  const assert = (cond, msg) => { if (!cond) throw new Error('断言失败: ' + msg); };

  if (!input) {
    // 无参数：验证草稿发现与名称读取
    const { drafts } = await maps.collectDrafts();
    assert(drafts.length > 0, '应发现草稿');
    assert(drafts.every((d) => d.name && d.name.length), '每个草稿应有名称');
    console.log(`PASS  发现 ${drafts.length} 个草稿:`);
    for (const d of drafts) console.log(`       - ${d.name}  (${d.folder})`);
    // 按名称解析（第一个草稿名）
    const hit = maps.matchByName(drafts, drafts[0].name);
    assert(hit.length === 1, '按名称应唯一命中');
    console.log(`PASS  按名称解析: "${drafts[0].name}" -> ${hit[0].folder}`);
    return;
  }

  const scene = await loadScene(input);
  const stats = countNodes(scene);
  assert(stats.count > 0, '节点数应 > 0');
  assert(stats.canvasCount > 0, '应至少有一个画布');
  console.log(`PASS  ${input}  nodes=${stats.count}  canvases=${stats.canvasCount}  depth=${stats.maxDepth}`);

  // 新导出部件：UITree.md 行数 == 节点数；node_index 覆盖全部节点
  const treeLines = treeMarkdown(scene);
  const index = buildNodeIndex(scene);
  const n = countAll(scene.nodes);
  assert(treeLines.length === n, `UITree 行数(${treeLines.length})应等于节点数(${n})`);
  assert(Object.keys(index).length === n, `node_index 条目(${Object.keys(index).length})应等于节点数(${n})`);
  console.log(`PASS  UITree.md 行数=${treeLines.length}  node_index 覆盖=${Object.keys(index).length}`);
}

main().catch((e) => {
  console.error('FAIL', e.message);
  process.exit(1);
});
