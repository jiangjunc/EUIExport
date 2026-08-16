'use strict';

/**
 * 生成随导出一起产出的 README.md：
 * - 告诉 AI 如何先读 UITree.md，再索引到 UIData.json 查节点细节
 * - 说明 UIData.json 里每一个字段的含义
 */

function esc(s) {
  return String(s === undefined || s === null ? '' : s).replace(/\|/g, '\\|').replace(/\n/g, ' ');
}

function buildReadme({ mapName, folder, sourceFile, stats, hasNodeIndex }) {
  const counts = stats || {};
  const lines = [];

  lines.push(`# 地图 UI 说明 · ${mapName || '(未命名)'}`);
  lines.push('');
  lines.push(`- **节点总数**：${counts.count ?? '?'} ｜ **画布**：${counts.canvasCount ?? '?'} ｜ **最大层级深度**：${counts.maxDepth ?? '?'} ｜ **系统控件/预置面板**：${counts.joystick_count ?? '?'}`);
  lines.push(`- **数据来源**：${esc(sourceFile || '')}`);
  lines.push('');

  lines.push('## 一、如何阅读这份导出（重要，请按顺序）');
  lines.push('');
  lines.push('1. **先读 `UITree.md`** —— 它是一棵完整的文本树状图：从根画布到每一个叶子节点，展示了所有 UI 节点的嵌套关系、名称、类型、id 与关键信息（文本内容 / 图片资源 / 透明度等）。先看它，你就能在几秒内把握整张界面的结构与层级。');
  lines.push('2. **再到 `UIData.json` 查细节** —— 在 `scene.nodes`（根画布列表）中按 `children` 递归下钻，找到你在 `UITree.md` 里感兴趣的节点（用它的 `id` 精确定位），该节点对象内就是它的**全部参数**（布局、显隐、文本、颜色、事件、行为…）。');
  lines.push('3. **快速定位用 `node_index`** —— `UIData.json` 顶部有 `node_index` 映射：输入任意节点 `id`，即可得到它的名称、类型与从画布到它的完整路径（如 `主游戏 > 功能 > 关卡显示`），避免手动递归。');
  lines.push('4. 任何字段的含义见本文档「三、字段字典」；节点里**未归类**的原始字段都会保留在该节点的 `other` 对象中。');
  lines.push('');

  lines.push('## 二、文件结构');
  lines.push('');
  lines.push('| 文件 | 作用 |');
  lines.push('| --- | --- |');
  lines.push('| `UIData.json` | **完整节点数据**：`summary` 统计 + `node_index` 快速索引 + `scene` 完整 UI 树（每个节点含全部参数） |');
  lines.push('| `UITree.md` | **完整节点树索引**：文本结构树状图，快速浏览层级、定位节点 id |');
  lines.push('| `README.md` | 本文件：阅读指南 + 字段字典 |');
  lines.push('');

  lines.push('## 三、基本概念');
  lines.push('');
  lines.push('- 编辑器 UI 的层级 = **画布(Canvas) → 容器/面板(Container) → 控件(文本/图片/…)**；系统控件（摇杆/按钮）与预置面板也挂在同一棵树上。');
  lines.push('- 每个节点的 `children` 数组就是它的子节点，**数组顺序 = 渲染/绘制顺序**（越靠后越在上层）。');
  lines.push('- 坐标原点在**画布左上角**；`pos` 是锚点所在坐标；`anchor` 是归一化锚点（0.5, 0.5 = 居中）。');
  lines.push('- 颜色统一为对象 `{ r, g, b, a?, hex }`，`hex` 形如 `#rrggbb` 或 `#rrggbbaa`。');
  lines.push('- 文本内容在 `text.text`；图片资源在 `image.image_path`（以 `gui2/...` 开头的内置资源包路径）。');
  lines.push('');

  lines.push('## 四、字段字典');
  lines.push('');
  lines.push('> `type` 可读名与 `type_code` 数字编码的对照：200=Canvas(画布) 10002=Text(文本) 10003=Image(图片) 10012=Container(容器) 20000=SystemControl(系统控件) 21000~21106=PresetPanel(预置面板)。');
  lines.push('');

  // ---- 通用字段 ----
  lines.push('### 4.1 节点通用字段（所有节点都有）');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `id` | 节点唯一 id（字符串数字）。跨文件索引用，也是 `node_index` 的键 |');
  lines.push('| `name` | 节点在编辑器里显示的名字 |');
  lines.push('| `type` | 控件类型可读名：Canvas / Text / Image / Container / SystemControl / PresetPanel / Unknown |');
  lines.push('| `type_code` | 控件类型的原始数字编码（见上方对照） |');
  lines.push('| `type_zh` | 类型中文说明 |');
  lines.push('| `cc_type` | 编辑器内部控件类型字符串（Layout / ImageView / Text / …） |');
  lines.push('| `z_order` | 层级顺序，数值越大越靠上 |');
  lines.push('| `parent` | 父节点 id（根画布为空字符串） |');
  lines.push('');

  // ---- layout ----
  lines.push('### 4.2 `layout` —— 布局与几何');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `anchor` | 锚点 `{x, y}`（0~1 归一化，0.5,0.5=居中；也可能带第 3 个分量） |');
  lines.push('| `pos` | 锚点所在坐标 `{x, y}`（画布左上角为原点） |');
  lines.push('| `size` | 节点尺寸 `{w, h}`（画布节点给的是设计分辨率） |');
  lines.push('| `rotation` | 旋转角度（度） |');
  lines.push('| `opacity` | 整体透明度（0~1，1=不透明） |');
  lines.push('| `flip_x` / `flip_y` | 是否水平 / 垂直翻转 |');
  lines.push('| `auto_adaption` | 自适应规则（数值编码） |');
  lines.push('| `need_horizontal` | 是否需要横向适配（布尔） |');
  lines.push('');

  // ---- visibility ----
  lines.push('### 4.3 `visibility` —— 显隐规则');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `default_show` | 默认是否显示 |');
  lines.push('| `default_show_by_player` | 按玩家默认显示（配合 player_enabled 开关） |');
  lines.push('| `default_show_by_player_enabled` | 是否启用「按玩家显示」 |');
  lines.push('| `spectate_hide` | 观战视角下是否隐藏 |');
  lines.push('| `take_photo_hide` | 拍照/截图时是否隐藏 |');
  lines.push('| `click_auto_hide` | 点击后是否自动隐藏 |');
  lines.push('| `auto_hide_enabled` | 是否启用自动隐藏 |');
  lines.push('| `auto_hide_time` | 自动隐藏延迟时间（秒） |');
  lines.push('| `editor_visible` | 编辑器中的可见性（不影响运行时） |');
  lines.push('| `only_level_id` | 仅指定关卡显示（关卡 id 数组） |');
  lines.push('| `vertical_screen` | 是否竖屏专属 |');
  lines.push('| `visible` | 运行时显隐布尔 |');
  lines.push('');

  // ---- text ----
  lines.push('### 4.4 `text` —— 文本（仅 Text 节点，type=10002）');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `text` | 文本内容（富文本以 `#f(...)`、`#l` 等标签保留原样） |');
  lines.push('| `font_path` / `font_names` | 字体资源路径 / 可用字体列表 |');
  lines.push('| `font_size` | 字号 |');
  lines.push('| `text_color` | 文字颜色 `{r,g,b,a,hex}` |');
  lines.push('| `text_h_alignment` | 水平对齐 `{value, label}`（left / center / right） |');
  lines.push('| `text_v_alignment` | 垂直对齐 `{value, label}`（top / center / bottom） |');
  lines.push('| `text_spacing` | 字间距 |');
  lines.push('| `line_spacing` | 行间距 |');
  lines.push('| `line_count` | 文本行数（容器节点上可能是网格列数） |');
  lines.push('| `min_font_size` | 最小字号（自动缩放下限） |');
  lines.push('| `enable_italic` | 是否斜体 |');
  lines.push('| `enable_outline` / `outline_color` / `outline_opacity` / `outline_width` | 描边开关 / 颜色 / 不透明度 / 宽度 |');
  lines.push('| `enable_shadow` / `shadow_color` / `shadow_posx` / `shadow_posy` | 阴影开关 / 颜色 / 偏移 x / 偏移 y |');
  lines.push('| `type_writer_enabled` / `type_writer_speed` | 打字机效果开关 / 速度 |');
  lines.push('| `rich_mode` | 富文本模式开关 |');
  lines.push('| `overflow_strategy` | 文本溢出策略（数值编码） |');
  lines.push('| `num_auto_scroll_enabled` | 数字自动滚动开关（计分牌类文本） |');
  lines.push('| `reset_size_policy` | 自动重置尺寸策略 |');
  lines.push('| `label_bind_attr` 及 related 系列 | 文本绑定属性（血量/分数等动态数值，`label_bind_attr_type`=来源、`_sub_type`=子类型、`_related_player_role_id`=绑定角色、`_related_creature_id`=绑定生物） |');
  lines.push('| `label_equipment_bind_attr` | 绑定装备属性 |');
  lines.push('');

  // ---- image ----
  lines.push('### 4.5 `image` —— 图片（Image 节点，type=10003）');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `image_path` | 图片资源路径（`gui2/...`） |');
  lines.push('| `view_image_path` | 预览/备用图片路径 |');
  lines.push('| `image_color` | 图片着色/染色颜色 `{r,g,b,a,hex}` |');
  lines.push('| `stretch_area` | 九宫格拉伸区域（数值编码） |');
  lines.push('');

  // ---- panel / background ----
  lines.push('### 4.6 `panel` / `background` —— 容器与背景外观');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `background_color` | 背景颜色 `{r,g,b,a,hex}` |');
  lines.push('| `background_opacity` | 背景透明度（0~1） |');
  lines.push('| `corner_radius` | 圆角半径 |');
  lines.push('| `polygon_type` / `polygon_points` | 多边形面板类型 / 顶点坐标 |');
  lines.push('| `line_color` / `line_width` / `line_count` | 边框颜色 / 线宽 / 线数（描边相关） |');
  lines.push('| `fill_color` | 填充颜色 |');
  lines.push('| `gradient_enabled` / `gradient_colors` / `gradient_degree` | 渐变开关 / 渐变颜色列表 / 渐变角度 |');
  lines.push('');

  // ---- events ----
  lines.push('### 4.7 `events` —— 事件与音频');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `event_list` | 事件列表（每条含名称/参数） |');
  lines.push('| `touch_begin_event` / `touch_click_event` / `touch_end_event` / `long_touch_event` | 触摸开始 / 点击 / 触摸结束 / 长按 时触发的事件 |');
  lines.push('| `show_event` / `hide_event` | 显示 / 隐藏 时触发的事件 |');
  lines.push('| `show_joystick_event` / `hide_joystick_event` | 显示 / 隐藏摇杆时触发的事件 |');
  lines.push('| `touch_begin_audio` / `touch_click_audio` / `touch_end_audio` | 触摸开始 / 点击 / 结束 音效 |');
  lines.push('| `reset_anim_event` | 重置动画事件 |');
  lines.push('| `local_ui_event` | 本地 UI 事件 |');
  lines.push('| `send_event_list` / `listen_event_list` | 发送 / 监听的广播事件名列表 |');
  lines.push('| `scene_3d_ui_listen_event_list` | 场景 3D UI 监听事件列表 |');
  lines.push('');

  // ---- behavior ----
  lines.push('### 4.8 `behavior` —— 交互与行为');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `touch_enabled` | 是否响应触摸（非按钮类型默认关闭） |');
  lines.push('| `touch_drag_enabled` | 是否可拖拽 |');
  lines.push('| `long_touch_time` | 长按判定时间（秒） |');
  lines.push('| `animations` / `default_play_anim` | 动画列表 / 默认播放的动画 |');
  lines.push('| `preset_style` | 预置样式（数值编码） |');
  lines.push('| `is_locked` / `lock` | 是否锁定（编辑器内不可编辑） |');
  lines.push('| `has_name_changed` / `native_layout_has_changed` | 内部标记（名称 / 原生布局是否被改动过） |');
  lines.push('| `simplified_resize_mode` | 简化自适应模式（数值编码） |');
  lines.push('| `enable_auto_size` | 是否自动尺寸 |');
  lines.push('');

  // ---- other ----
  lines.push('### 4.9 `other` —— 未归类的原始字段');
  lines.push('');
  lines.push('未被上述分组认领的字段原样保留在这里（值已做可读化处理），用于不丢失任何信息。');
  lines.push('');

  // ---- scene level ----
  lines.push('### 4.10 `scene`（UIData.json 顶层）');
  lines.push('');
  lines.push('| 字段 | 含义 |');
  lines.push('| --- | --- |');
  lines.push('| `type` | 固定为 `Scene` |');
  lines.push('| `version` | 场景版本字符串 |');
  lines.push('| `id` | 场景/地图 id |');
  lines.push('| `canvas_layer_ids` | 画布分层 id 列表 |');
  lines.push('| `design_size` | 设计分辨率 `{w,h}` |');
  lines.push('| `vertical_screen` | 是否竖屏设计 |');
  lines.push('| `simplified_resize_mode` | 画布自适应模式（数值编码） |');
  lines.push('| `joystick_count` | 系统控件/预置面板数量 |');
  lines.push('| `nodes` | 根画布节点数组（整棵 UI 树的入口） |');
  lines.push('');

  lines.push('## 五、坐标系补充说明');
  lines.push('');
  lines.push('- 画布节点（Canvas）的 `size` 即设计分辨率（如 2210×1080），所有子节点坐标都相对它。');
  lines.push('- 文本对齐：`text_h_alignment` 的 `label` 为 `left/center/right`，`text_v_alignment` 的 `label` 为 `top/center/bottom`；原始数字保留在 `value` 中。');
  lines.push('- 长文本内容（多行/富文本）在存储层为 base64，导出时已自动解码为可读文本。');
  lines.push('');

  lines.push('---');
  lines.push(`> 由蛋仔 EUI 导出器生成 · 生成时间：${new Date().toISOString()}`);
  lines.push('');

  return lines.join('\n');
}

module.exports = { buildReadme };
