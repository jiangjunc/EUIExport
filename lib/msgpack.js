'use strict';

/**
 * 极简 MessagePack 解码器（覆盖蛋仔编辑器地图数据实际用到的类型）
 * - fixint / fixmap / fixarray / fixstr
 * - nil / false / true
 * - bin8/16/32 (0xC4/0xC5/0xC6，蛋仔用作字符串)
 * - float32/64、uint8/16/32/64、int8/16/32/64
 * - str8/16/32 (0xD9/0xDA/0xDB)
 * - array16/32、map16/32
 */

function toSafeNum(big) {
  const n = Number(big);
  return Number.isSafeInteger(n) ? n : big.toString();
}

function decode(bytes, pos) {
  if (pos >= bytes.length) throw new Error('Unexpected end of MessagePack buffer');
  const b = bytes[pos];

  if (b <= 0x7f) return { v: b, n: 1 };                  // positive fixint
  if (b >= 0xe0) return { v: b - 256, n: 1 };            // negative fixint
  if (b >= 0x80 && b <= 0x8f) {                          // fixmap
    const cnt = b - 0x80; let p = pos + 1; const m = {};
    for (let i = 0; i < cnt; i++) {
      const k = decode(bytes, p); p += k.n;
      const v = decode(bytes, p); p += v.n;
      m[k.v] = v.v;
    }
    return { v: m, n: p - pos };
  }
  if (b >= 0x90 && b <= 0x9f) {                          // fixarray
    const cnt = b - 0x90; let p = pos + 1; const a = new Array(cnt);
    for (let i = 0; i < cnt; i++) { const e = decode(bytes, p); p += e.n; a[i] = e.v; }
    return { v: a, n: p - pos };
  }
  if (b >= 0xa0 && b <= 0xbf) {                          // fixstr
    return { v: bytes.toString('utf8', pos + 1, pos + 1 + (b - 0xa0)), n: 1 + (b - 0xa0) };
  }

  switch (b) {
    case 0xc0: return { v: null, n: 1 };
    case 0xc2: return { v: false, n: 1 };
    case 0xc3: return { v: true, n: 1 };
    case 0xc4: { const l = bytes[pos + 1]; return { v: bytes.toString('utf8', pos + 2, pos + 2 + l), n: 2 + l }; } // bin8 -> string
    case 0xc5: { const l = bytes.readUInt16BE(pos + 1); return { v: bytes.subarray(pos + 3, pos + 3 + l).toString('base64'), n: 3 + l }; }
    case 0xc6: { const l = bytes.readUInt32BE(pos + 1); return { v: bytes.subarray(pos + 5, pos + 5 + l).toString('base64'), n: 5 + l }; }
    case 0xca: return { v: bytes.readFloatBE(pos + 1), n: 5 };
    case 0xcb: return { v: bytes.readDoubleBE(pos + 1), n: 9 };
    case 0xcc: return { v: bytes[pos + 1], n: 2 };
    case 0xcd: return { v: bytes.readUInt16BE(pos + 1), n: 3 };
    case 0xce: return { v: bytes.readUInt32BE(pos + 1), n: 5 };
    case 0xcf: return { v: toSafeNum(bytes.readBigUInt64BE(pos + 1)), n: 9 };
    case 0xd0: return { v: bytes.readInt8(pos + 1), n: 2 };
    case 0xd1: return { v: bytes.readInt16BE(pos + 1), n: 3 };
    case 0xd2: return { v: bytes.readInt32BE(pos + 1), n: 5 };
    case 0xd3: return { v: toSafeNum(bytes.readBigInt64BE(pos + 1)), n: 9 };
    case 0xd9: { const l = bytes[pos + 1]; return { v: bytes.toString('utf8', pos + 2, pos + 2 + l), n: 2 + l }; } // str8
    case 0xda: { const l = bytes.readUInt16BE(pos + 1); return { v: bytes.toString('utf8', pos + 3, pos + 3 + l), n: 3 + l }; }
    case 0xdb: { const l = bytes.readUInt32BE(pos + 1); return { v: bytes.toString('utf8', pos + 5, pos + 5 + l), n: 5 + l }; }
    case 0xdc: { const l = bytes.readUInt16BE(pos + 1); let p = pos + 3; const a = new Array(l);
      for (let i = 0; i < l; i++) { const e = decode(bytes, p); p += e.n; a[i] = e.v; }
      return { v: a, n: p - pos }; }
    case 0xdd: { const l = bytes.readUInt32BE(pos + 1); let p = pos + 5; const a = new Array(l);
      for (let i = 0; i < l; i++) { const e = decode(bytes, p); p += e.n; a[i] = e.v; }
      return { v: a, n: p - pos }; }
    case 0xde: { const l = bytes.readUInt16BE(pos + 1); let p = pos + 3; const m = {};
      for (let i = 0; i < l; i++) { const k = decode(bytes, p); p += k.n; const v = decode(bytes, p); p += v.n; m[k.v] = v.v; }
      return { v: m, n: p - pos }; }
    case 0xdf: { const l = bytes.readUInt32BE(pos + 1); let p = pos + 5; const m = {};
      for (let i = 0; i < l; i++) { const k = decode(bytes, p); p += k.n; const v = decode(bytes, p); p += v.n; m[k.v] = v.v; }
      return { v: m, n: p - pos }; }
    default:
      throw new Error(`Unsupported MessagePack tag 0x${b.toString(16)} at byte ${pos}`);
  }
}

/** 解码整段 buffer，严格校验全部消费 */
function decodeAll(bytes) {
  const { v, n } = decode(bytes, 0);
  if (n !== bytes.length) {
    throw new Error(`MessagePack 解码未完整消费: ${n}/${bytes.length} bytes`);
  }
  return v;
}

/* ------------------------------------------------------------------ *
 *  编码器（与上方解码器对称，用于修改 .mm 后回写）
 *  数值策略：整数用最小编码（fixint/int8/16/32/64），浮点用 float64；
 *  字符串：fixstr/str8/str16/str32；数组/映射：fix/16/32。
 * ------------------------------------------------------------------ */

function pushNum(arr, v) {
  if (v >= 0) {
    if (v <= 0x7f) { arr.push(v); return; }
    if (v <= 0xff) { arr.push(0xcc, v); return; }
    if (v <= 0xffff) { arr.push(0xcd, v >> 8, v & 0xff); return; }
    if (v <= 0xffffffff) { arr.push(0xce, (v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff); return; }
    // uint64
    const big = BigInt(v);
    arr.push(0xcf);
    for (let i = 7; i >= 0; i--) arr.push(Number((big >> BigInt(i * 8)) & 0xffn));
    return;
  }
  if (v >= -32) { arr.push(0xe0 + (v + 32)); return; }
  if (v >= -128) { arr.push(0xd0, v & 0xff); return; }
  if (v >= -32768) { arr.push(0xd1, (v >> 8) & 0xff, v & 0xff); return; }
  if (v >= -2147483648) { arr.push(0xd2, (v >>> 24) & 0xff, (v >>> 16) & 0xff, (v >>> 8) & 0xff, v & 0xff); return; }
  const big = BigInt(v);
  arr.push(0xd3);
  for (let i = 7; i >= 0; i--) arr.push(Number((big >> BigInt(i * 8)) & 0xffn));
}

function encodeNumber(arr, v) {
  if (Number.isInteger(v)) { pushNum(arr, v); return; }
  const b = Buffer.alloc(8);
  b.writeDoubleBE(v);
  arr.push(0xcb, ...b);
}

function encodeString(arr, s) {
  const b = Buffer.from(s, 'utf8');
  const len = b.length;
  if (len <= 31) { arr.push(0xa0 + len, ...b); return; }
  if (len <= 0xff) { arr.push(0xd9, len, ...b); return; }
  if (len <= 0xffff) { arr.push(0xda, (len >> 8) & 0xff, len & 0xff, ...b); return; }
  arr.push(0xdb, (len >>> 24) & 0xff, (len >>> 16) & 0xff, (len >>> 8) & 0xff, len & 0xff, ...b);
}

function encodeValue(arr, v) {
  if (v === null || v === undefined) { arr.push(0xc0); return; }
  const t = typeof v;
  if (t === 'boolean') { arr.push(v ? 0xc3 : 0xc2); return; }
  if (t === 'number') { encodeNumber(arr, v); return; }
  if (t === 'string') { encodeString(arr, v); return; }
  if (t === 'bigint') { encodeNumber(arr, Number(v)); return; }
  if (Array.isArray(v)) {
    const len = v.length;
    if (len <= 15) arr.push(0x90 + len);
    else if (len <= 0xffff) arr.push(0xdc, (len >> 8) & 0xff, len & 0xff);
    else arr.push(0xdd, (len >>> 24) & 0xff, (len >>> 16) & 0xff, (len >>> 8) & 0xff, len & 0xff);
    for (const e of v) encodeValue(arr, e);
    return;
  }
  if (t === 'object') {
    const keys = Object.keys(v);
    const len = keys.length;
    if (len <= 15) arr.push(0x80 + len);
    else if (len <= 0xffff) arr.push(0xde, (len >> 8) & 0xff, len & 0xff);
    else arr.push(0xdf, (len >>> 24) & 0xff, (len >>> 16) & 0xff, (len >>> 8) & 0xff, len & 0xff);
    for (const k of keys) { encodeString(arr, k); encodeValue(arr, v[k]); }
    return;
  }
  throw new Error(`Unsupported value type: ${t}`);
}

/** 把 JS 值编码为 MessagePack 字节 */
function encode(value) {
  const arr = [];
  encodeValue(arr, value);
  return Buffer.from(arr);
}

module.exports = { decode, decodeAll, encode };
