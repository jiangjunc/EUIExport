'use strict';

/**
 * eui-edit 冒烟测试：
 *  - 解码 eui.mm -> 修改字段 -> msgpack 重编码 -> zstd 压缩 -> 解压 -> 再解码
 *  - 断言：目标字段已改、其余字段与原始完全一致（往返保真）
 * 用法：node test/edit-smoke.js <地图工程目录|草稿名称|base64 id>
 */

const fs = require('fs');
const path = require('path');
const { ZSTDDecoder } = require('../lib/zstd.cjs');
const { decodeAll, encode } = require('../lib/msgpack');
const zstdCodec = require('../lib/zstd-codec/zstd-codec.js');
const maps = require('../lib/maps');

async function main() {
  const input = process.argv[2];
  const cwd = process.cwd();
  const assert = (cond, msg) => { if (!cond) throw new Error('断言失败: ' + msg); };

  // 定位地图
  let dir = null;
  if (input) {
    if (fs.existsSync(input) && fs.statSync(input).isDirectory()) dir = input;
    else {
      const { drafts } = await maps.collectDrafts();
      const hit = maps.matchByName(drafts, input);
      if (hit.length === 1) dir = hit[0].dir;
      else throw new Error(`无法按“${input}”定位地图`);
    }
  } else {
    const { drafts } = await maps.collectDrafts();
    if (!drafts.length) throw new Error('未发现草稿');
    dir = drafts[0].dir;
    console.log(`（未指定地图，用首个草稿: ${drafts[0].name}）`);
  }
  const mm = path.join(dir, 'eui.mm');
  assert(fs.existsSync(mm), `缺少 ${mm}`);

  // 解码
  const dec = new ZSTDDecoder();
  await dec.init();
  const raw = Buffer.from(dec.decode(new Uint8Array(fs.readFileSync(mm))));
  const treeA = decodeAll(raw);
  const scene = treeA[0];
  const nodes = scene.nodes;
  assert(Array.isArray(nodes) && nodes.length > 0, '应有节点');

  // 修改：目标节点 name + size[0]
  const before = JSON.stringify(treeA); // 改前快照
  const target = nodes[0];
  const oldName = target.name;
  const newName = oldName + '_EDIT_TEST';
  const newSize = target.size[0] + 1;
  target.name = newName;
  target.size[0] = newSize;
  target.extra = target.extra || {};
  target.extra.__edit_test = { ok: true, n: 42 };

  // 重编码 + 压缩
  const encoded = encode(treeA);
  const compressed = await new Promise((resolve, reject) => {
    zstdCodec.run((codec) => {
      try { resolve(Buffer.from(new codec.Simple().compress(new Uint8Array(encoded)))); }
      catch (e) { reject(e); }
    });
  });

  // 解压 + 再解码
  const raw2 = Buffer.from(dec.decode(new Uint8Array(compressed)));
  const treeB = decodeAll(raw2);
  const sceneB = treeB[0];
  const targetB = sceneB.nodes[0];

  // 断言
  assert(sceneB.nodes.length === nodes.length, '节点数不变');
  assert(targetB.name === newName, `name 应改为 ${newName}`);
  assert(targetB.size[0] === newSize, `size[0] 应改为 ${newSize}`);
  assert(targetB.extra.__edit_test.ok === true && targetB.extra.__edit_test.n === 42, '新增字段应保留');
  assert(treeB[0].design_size.join() === treeA[0].design_size.join(), '其余字段不变');
  assert(JSON.stringify(treeB) !== before, '内容应确实发生变化');
  // 除目标节点外，其余节点应完全一致
  for (let i = 1; i < sceneB.nodes.length; i++) {
    assert(JSON.stringify(sceneB.nodes[i]) === JSON.stringify(scene.nodes[i]), `节点 ${i} 不应被改动`);
  }

  console.log(`PASS  往返修改成功: ${mm}`);
  console.log(`      name "${oldName}" -> "${newName}"  |  size[0] -> ${newSize}`);
  console.log(`      压缩后 ${compressed.length} B（原 ${fs.statSync(mm).size} B）`);
}

main()
  .catch((e) => {
    console.error('FAIL', e.message);
    setTimeout(() => process.exit(1), 60);
  })
  .then(() => {
    // zstddec(wasm)+zstd-codec 同时加载后，Node 自然退出会触发 libuv 断言崩溃；
    // 用强制退出规避（与 eui-edit.js 一致）。
    setTimeout(() => process.exit(0), 60);
  });
