'use strict';

/**
 * 蛋仔 EUI 节点模型与「人/机可读化」导出逻辑
 *
 * 输入：eui.mm 解压并 MessagePack 解码后的原始树
 * 输出：结构清晰、字段可读、层级嵌套明确的 UI 树 JSON
 */

/* ------------------------------------------------------------------ *
 *  控件类型表（type 数字 -> 可读名称）
 * ------------------------------------------------------------------ */
const TYPE_MAP = {
  200:    { name: 'Canvas',        zh: '画布(根)' },
  10002:  { name: 'Text',          zh: '文本' },
  10003:  { name: 'Image',         zh: '图片' },
  10012:  { name: 'Container',     zh: '容器/面板' },
  20000:  { name: 'SystemControl', zh: '系统控件(摇杆/按钮)' },
  21000:  { name: 'PresetPanel',   zh: '预置面板' },
  21101:  { name: 'PresetPanel',   zh: '预置面板' },
  21102:  { name: 'PresetPanel',   zh: '预置面板' },
  21103:  { name: 'PresetPanel',   zh: '预置面板' },
  21104:  { name: 'PresetPanel',   zh: '预置面板' },
  21105:  { name: 'PresetPanel',   zh: '预置面板' },
  21106:  { name: 'PresetPanel',   zh: '预置面板' },
};

/* ------------------------------------------------------------------ *
 *  字段分组（用于把原始字段归入可读分组）
 * ------------------------------------------------------------------ */
const LAYOUT_FIELDS = ['anchor', 'pos', 'size', 'rotation', 'opacity', 'flip_x', 'flip_y', 'auto_adaption', 'need_horizontal'];
const VISIBILITY_FIELDS = [
  'default_show', 'default_show_by_player', 'default_show_by_player_enabled',
  'spectate_hide', 'take_photo_hide', 'click_auto_hide', 'auto_hide_enabled', 'auto_hide_time',
  'editor_visible', 'only_level_id', 'vertical_screen', 'visible',
];
const EVENT_FIELDS = [
  'event_list', 'touch_begin_event', 'touch_click_event', 'touch_end_event', 'long_touch_event',
  'show_event', 'hide_event', 'show_joystick_event', 'hide_joystick_event',
  'touch_begin_audio', 'touch_click_audio', 'touch_end_audio', 'reset_anim_event',
  'local_ui_event', 'send_event_list', 'listen_event_list', 'scene_3d_ui_listen_event_list',
];
const BEHAVIOR_FIELDS = [
  'touch_enabled', 'touch_drag_enabled', 'long_touch_time', 'animations', 'default_play_anim',
  'preset_style', 'is_locked', 'lock', 'has_name_changed', 'native_layout_has_changed',
  'simplified_resize_mode', 'enable_auto_size',
];
const TEXT_FIELDS = [
  'text', 'font_path', 'font_names', 'font_size', 'text_color',
  'text_h_alignment', 'text_v_alignment', 'text_spacing', 'line_spacing', 'line_count',
  'min_font_size', 'enable_italic', 'enable_outline', 'outline_color', 'outline_opacity', 'outline_width',
  'enable_shadow', 'shadow_color', 'shadow_posx', 'shadow_posy',
  'type_writer_enabled', 'type_writer_speed', 'rich_mode', 'overflow_strategy',
  'num_auto_scroll_enabled', 'reset_size_policy',
  'label_bind_attr', 'label_bind_attr_type', 'label_bind_attr_sub_type',
  'label_bind_attr_related_player_role_id', 'label_bind_attr_related_creature_id', 'label_equipment_bind_attr',
];
const IMAGE_FIELDS = ['image_path', 'view_image_path', 'image_color', 'stretch_area'];
const PANEL_FIELDS = [
  'background_color', 'background_opacity', 'corner_radius', 'polygon_type', 'polygon_points',
  'line_color', 'line_width', 'line_count', 'fill_color',
  'gradient_enabled', 'gradient_colors', 'gradient_degree',
];

const USED_KEYS = new Set([
  'id', 'name', 'type', 'cc_type', 'z_order', 'parent', 'nodes', 'inner_children',
  ...LAYOUT_FIELDS, ...VISIBILITY_FIELDS, ...EVENT_FIELDS, ...BEHAVIOR_FIELDS,
  ...TEXT_FIELDS, ...IMAGE_FIELDS, ...PANEL_FIELDS,
]);

/* ------------------------------------------------------------------ *
 *  值可读化
 * ------------------------------------------------------------------ */
const VEC_TAG = '\u0011';                       // 数组末尾的类型标记字节 (0x11)
const isTagged = (v) => Array.isArray(v) && v.length > 0 && typeof v[v.length - 1] === 'string' && v[v.length - 1] === VEC_TAG;
const round = (n) => (typeof n === 'number' ? Math.round(n * 1000) / 1000 : n);

function colorObj(arr) {
  const a = arr.map(round);
  const r = Math.round(a[0]), g = Math.round(a[1]), b = Math.round(a[2]);
  const hasAlpha = a.length >= 4;
  const alpha = hasAlpha ? Math.round(a[3]) : 255;
  let hex = '#' + [r, g, b].map((x) => x.toString(16).padStart(2, '0')).join('');
  if (hasAlpha && alpha < 255) hex += alpha.toString(16).padStart(2, '0');
  const o = { r, g, b, hex };
  if (hasAlpha) o.a = alpha;
  return o;
}

const H_ALIGN = { 0: 'left', 1: 'center', 2: 'right' };
const V_ALIGN = { 0: 'top', 1: 'center', 2: 'bottom' };

/**
 * 蛋仔编辑器里长文本（尤其带换行的）以 base64(UTF-8) 字符串存贮。
 * 这里做严格检测并解码：仅当字符串符合 base64 特征、解码后是合法 UTF-8
 * 且包含中文/全角字符时才解码，避免误伤普通字符串（资源路径、id 等）。
 */
function tryDecodeBase64Text(s) {
  if (typeof s !== 'string' || s.length < 16) return s;
  if (!/^[A-Za-z0-9+/=\r\n]+$/.test(s)) return s;          // base64 字符集
  let pad = s;
  while (pad.length % 4 !== 0) pad += '=';
  let buf;
  try { buf = Buffer.from(pad, 'base64'); } catch { return s; }
  const dec = buf.toString('utf8');
  // 合法 UTF-8 校验：再编码后应与原字节一致
  if (Buffer.from(dec, 'utf8').equals(buf.subarray(0, Buffer.byteLength(dec)))) {
    // 可打印字符占比 + 含中文/全角
    let printable = 0;
    for (const ch of dec) {
      const code = ch.codePointAt(0);
      if ((code >= 0x20 && code <= 0x7e) || code >= 0x80) printable++;
    }
    if (printable / Math.max(1, dec.length) > 0.9 &&
        /[\u4e00-\u9fff\u3000-\u303f\uff00-\uffef]/.test(dec)) {
      return dec;
    }
  }
  return s;
}

function humanize(value, key) {
  if (value === null || value === undefined) return value;
  const t = typeof value;
  if (t === 'number' || t === 'boolean') return value;
  if (t === 'string') return tryDecodeBase64Text(value);

  if (Array.isArray(value)) {
    const body = isTagged(value) ? value.slice(0, -1) : value;
    const allNum = body.length > 0 && body.every((n) => typeof n === 'number');

    // 颜色（仅当 key 与颜色相关时判定，避免与坐标混淆）
    if (allNum && (key === 'image_color' || key === 'text_color' || key === 'outline_color' ||
        key === 'shadow_color' || key === 'background_color' || key === 'fill_color' || key === 'line_color') &&
        (body.length === 3 || body.length === 4)) {
      return colorObj(body);
    }
    // 渐变颜色数组
    if (key === 'gradient_colors' && body.every((e) => Array.isArray(e))) {
      return body.map((e) => (e.length === 3 || e.length === 4) && e.every((n) => typeof n === 'number') ? colorObj(e) : e);
    }

    // 几何
    if (key === 'size' && body.length === 2 && allNum) return { w: round(body[0]), h: round(body[1]) };
    if ((key === 'pos' || key === 'anchor' || key === 'scale') && body.length === 2 && allNum) return { x: round(body[0]), y: round(body[1]) };
    if (body.length === 2 && allNum) return { x: round(body[0]), y: round(body[1]) };
    if (body.length === 3 && allNum) return { x: round(body[0]), y: round(body[1]), z: round(body[2]) };

    return value.map((e, i) => humanize(e, `${key}[${i}]`));
  }

  if (t === 'object') {
    const out = {};
    for (const [k, v] of Object.entries(value)) out[k] = humanize(v, k);
    return out;
  }
  return value;
}

/** 从原始节点挑选字段并可读化 */
function pickFields(raw, fieldList) {
  const out = {};
  for (const k of fieldList) {
    if (raw[k] === undefined) continue;
    let v = humanize(raw[k], k);
    // 对齐枚举可读化
    if (k === 'text_h_alignment' && typeof v === 'number') v = { value: v, label: H_ALIGN[v] || `unknown(${v})` };
    if (k === 'text_v_alignment' && typeof v === 'number') v = { value: v, label: V_ALIGN[v] || `unknown(${v})` };
    out[k] = v;
  }
  return out;
}

/* ------------------------------------------------------------------ *
 *  节点导出
 * ------------------------------------------------------------------ */
function exportNode(raw) {
  // 空节点守卫（个别地图可能含 null 占位节点）
  if (raw === null || raw === undefined) {
    return { id: '', name: '(空节点)', type: 'null', type_code: null };
  }
  const ti = TYPE_MAP[raw.type] || { name: `Unknown(${raw.type})`, zh: '未知类型' };
  const node = {
    id: raw.id !== undefined ? String(raw.id) : '',
    name: typeof raw.name === 'string' ? raw.name : '',
    type: ti.name,
    type_code: raw.type,
  };
  if (ti.zh) node.type_zh = ti.zh;
  if (raw.cc_type) node.cc_type = raw.cc_type;
  if (typeof raw.z_order === 'number') node.z_order = raw.z_order;
  if (raw.parent !== undefined) node.parent = String(raw.parent);

  const layout = pickFields(raw, LAYOUT_FIELDS);
  if (Object.keys(layout).length) node.layout = layout;

  const visibility = pickFields(raw, VISIBILITY_FIELDS);
  if (Object.keys(visibility).length) node.visibility = visibility;

  const image = pickFields(raw, IMAGE_FIELDS);
  if (Object.keys(image).length) node.image = image;

  const panel = pickFields(raw, PANEL_FIELDS);
  if (Object.keys(panel).length) node.panel = panel;

  const events = pickFields(raw, EVENT_FIELDS);
  if (Object.keys(events).length) node.events = events;

  const behavior = pickFields(raw, BEHAVIOR_FIELDS);
  if (Object.keys(behavior).length) node.behavior = behavior;

  // 文本控件专属：顶层样式 + inner_children 模板归位（Text 控件才有这些）
  const inner = raw.inner_children;
  if (raw.type === 10002) {
    const textTop = pickFields(raw, TEXT_FIELDS);
    if (inner && typeof inner === 'object') {
      if (inner.Text_content) node.text = { ...textTop, ...pickFields(inner.Text_content, TEXT_FIELDS) };
      if (inner.RichTextEx) node.rich_text = pickFields(inner.RichTextEx, TEXT_FIELDS);
      if (inner.Panel_bg) node.background = pickFields(inner.Panel_bg, [...PANEL_FIELDS, ...LAYOUT_FIELDS]);
    }
    if (!node.text && Object.keys(textTop).length) node.text = textTop;
  } else if (inner && typeof inner === 'object' && inner.Panel_bg) {
    node.background = pickFields(inner.Panel_bg, [...PANEL_FIELDS, ...LAYOUT_FIELDS]);
  }

  // 其余未归类字段（保留完整信息）
  const other = {};
  for (const k of Object.keys(raw)) {
    if (!USED_KEYS.has(k)) other[k] = humanize(raw[k], k);
  }
  if (Object.keys(other).length) node.other = other;

  // 子节点（嵌套 nodes 数组 = 编辑器内的层级）
  if (Array.isArray(raw.nodes) && raw.nodes.length) {
    node.children = raw.nodes.map(exportNode);
  }

  return node;
}

/* ------------------------------------------------------------------ *
 *  场景导出 + 统计
 * ------------------------------------------------------------------ */

/**
 * 收集场景里所有 UI 节点原始数据。
 * 蛋仔 eui.mm 结构：
 *   scene.nodes      -> 用户 UI（画布/容器/文本/图片，扁平存储）
 *   scene.joysticks  -> 系统控件与预置面板（摇杆/按钮/Panel_*，扁平存储）
 * 所有节点用 parent(id) 指针关联，层级需按 parent 重建。
 */
function collectRawNodes(sceneMap) {
  const all = [];
  const push = (k) => { if (Array.isArray(sceneMap[k])) all.push(...sceneMap[k]); };
  push('nodes');
  push('joysticks');
  // 兜底：若未来结构出现其它节点数组，在此扩展
  return all;
}

function exportScene(root) {
  let sceneMap = root;
  if (Array.isArray(root)) {
    // 数组形式：取第一个非空对象元素（个别 SE 变体可能 [null, scene]）
    sceneMap = root.find((el) => el !== null && el !== undefined && typeof el === 'object' && !Array.isArray(el)) || root[0];
  }
  if (sceneMap === null || sceneMap === undefined) {
    throw new Error('EUI 数据顶层为空（null），没有可导出的 UI');
  }
  if (typeof sceneMap !== 'object' || Array.isArray(sceneMap)) {
    throw new Error(`EUI 数据顶层结构异常（${Array.isArray(sceneMap) ? '数组' : typeof sceneMap}）`);
  }
  const scene = { type: sceneMap.type, version: sceneMap.version };
  if (sceneMap.id !== undefined) scene.id = String(sceneMap.id);
  if (Array.isArray(sceneMap.canvas_layer_ids)) scene.canvas_layer_ids = sceneMap.canvas_layer_ids;
  if (sceneMap.design_size !== undefined) scene.design_size = humanize(sceneMap.design_size, 'design_size');
  if (typeof sceneMap.vertical_screen === 'boolean') scene.vertical_screen = sceneMap.vertical_screen;
  if (typeof sceneMap.simplified_resize_mode === 'boolean') scene.simplified_resize_mode = sceneMap.simplified_resize_mode;

  // 收集全部节点并建立 parent -> children 索引（过滤 null 占位）
  const allRaw = collectRawNodes(sceneMap).filter((n) => n !== null && n !== undefined && typeof n === 'object');
  const allIds = new Set(allRaw.map((n) => String(n.id)));
  const childrenOf = new Map();
  for (const n of allRaw) {
    const pid = n.parent === undefined || n.parent === null || n.parent === '' ? '' : String(n.parent);
    if (!childrenOf.has(pid)) childrenOf.set(pid, []);
    childrenOf.get(pid).push(n);
  }

  // 根 = parent 为空，或 parent 指向不存在的节点
  const roots = allRaw.filter((n) => {
    const pid = n.parent === undefined || n.parent === null || n.parent === '' ? '' : String(n.parent);
    return pid === '' || !allIds.has(pid);
  });

  function buildNode(raw) {
    if (raw === null || raw === undefined || typeof raw !== 'object') {
      return exportNode(raw); // 空节点占位，不崩
    }
    const node = exportNode(raw);
    const id = String(raw.id);
    let kids = childrenOf.get(id) || [];
    // 兜底：个别节点可能用嵌套 nodes 数组存子节点
    if (!kids.length && Array.isArray(raw.nodes) && raw.nodes.length) kids = raw.nodes;
    if (kids.length) node.children = kids.map(buildNode);
    return node;
  }

  scene.nodes = roots.map(buildNode);
  scene.joystick_count = (sceneMap.joysticks && sceneMap.joysticks.length) || 0;
  return scene;
}

function countNodes(scene) {
  let count = 0;
  let maxDepth = 0;
  const typeCounts = {};
  (function walk(nodes, depth) {
    for (const n of nodes) {
      count++;
      maxDepth = Math.max(maxDepth, depth);
      typeCounts[n.type] = (typeCounts[n.type] || 0) + 1;
      if (n.children) walk(n.children, depth + 1);
    }
  })(scene.nodes, 0);
  return {
    count,
    maxDepth,
    typeCounts,
    canvasCount: typeCounts.Canvas || 0,
  };
}

/** 生成树预览文本（用于 CLI 控制台展示） */
function treePreview(scene, maxLines = 50) {
  const lines = [];
  (function walk(nodes, depth, prefix) {
    for (let i = 0; i < nodes.length; i++) {
      const n = nodes[i];
      const last = i === nodes.length - 1;
      const connector = depth === 0 ? '' : (last ? '└─ ' : '├─ ');
      const indent = depth === 0 ? '' : prefix + connector;
      let line = `${indent}${n.type_zh || n.type}${n.type_zh ? ' ' + n.type : ''}「${n.name || '(未命名)'}」`;
      if (n.text && typeof n.text.text === 'string' && n.text.text) line += ` 文本="${n.text.text}"`;
      if (n.image && n.image.image_path) line += ` img=${n.image.image_path}`;
      if (n.layout && typeof n.layout.opacity === 'number' && n.layout.opacity !== 1) line += ` α=${n.layout.opacity}`;
      lines.push(line);
      if (lines.length >= maxLines) return true; // 触顶
      if (n.children && n.children.length) {
        const childPrefix = depth === 0 ? '' : prefix + (last ? '   ' : '│  ');
        if (walk(n.children, depth + 1, childPrefix)) return true;
      }
    }
    return false;
  })(scene.nodes, 0, '');
  return lines;
}

/** 生成完整节点树文本（每行含 id 与关键信息，用于 UITree.md，不截断） */
function treeMarkdown(scene) {
  const lines = [];
  (function walk(nodes, depth, prefix) {
    for (let i = 0; i < nodes.length; i++) {
      const n = nodes[i];
      const last = i === nodes.length - 1;
      const connector = depth === 0 ? '' : (last ? '└─ ' : '├─ ');
      const indent = depth === 0 ? '' : prefix + connector;
      let line = `${indent}「${n.name || '(未命名)'}」 [${n.type} #${n.id}]`;
      if (n.text && typeof n.text.text === 'string' && n.text.text) {
        let t = n.text.text.replace(/\s+/g, ' ').trim();
        if (t.length > 40) t = t.slice(0, 40) + '…';
        line += `  文本="${t}"`;
      }
      if (n.image && n.image.image_path) line += `  img=${n.image.image_path}`;
      if (n.layout && typeof n.layout.opacity === 'number' && n.layout.opacity !== 1) line += `  α=${n.layout.opacity}`;
      if (n.background && n.background.background_color && typeof n.background.background_color.hex === 'string') line += `  背景=${n.background.background_color.hex}`;
      lines.push(line);
      if (n.children && n.children.length) {
        const childPrefix = depth === 0 ? '' : prefix + (last ? '   ' : '│  ');
        walk(n.children, depth + 1, childPrefix);
      }
    }
  })(scene.nodes, 0, '');
  return lines;
}

/** 生成 id -> {name, type, path} 快速索引（用于 UIData.json 的 node_index） */
function buildNodeIndex(scene) {
  const index = {};
  (function walk(nodes, breadcrumb) {
    for (const n of nodes) {
      const path = breadcrumb.concat(n.name || '(未命名)').join(' > ');
      index[n.id] = { name: n.name, type: n.type, type_zh: n.type_zh, path };
      if (n.children) walk(n.children, breadcrumb.concat(n.name || '(未命名)'));
    }
  })(scene.nodes, []);
  return index;
}

module.exports = {
  TYPE_MAP,
  exportNode,
  exportScene,
  countNodes,
  treePreview,
  treeMarkdown,
  buildNodeIndex,
  humanize,
  pickFields,
};
