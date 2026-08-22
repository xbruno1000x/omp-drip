#!/usr/bin/env node

import { createHash } from "node:crypto";
import { mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";

const args = new Map();
for (let index = 2; index < process.argv.length; index += 2) args.set(process.argv[index], process.argv[index + 1]);
const required = (name) => {
  const value = args.get(name);
  if (!value) throw new Error(`Missing required argument: ${name}`);
  return path.resolve(value);
};

const playerImgPath = required("--player-img");
const clothesDatPath = required("--clothes-dat");
const shoppingDatPath = required("--shopping-dat");
const overridesPath = args.get("--overrides") ? path.resolve(args.get("--overrides")) : null;
const basePlayerImgPath = args.get("--base-player-img") ? path.resolve(args.get("--base-player-img")) : null;
const outputRoot = path.resolve(args.get("--out") ?? "build/catalog");

const sha256 = (buffer) => createHash("sha256").update(buffer).digest("hex");

function stableId(slot, texture, model) {
  const source = `${slot}\0${texture.toLowerCase()}\0${model.toLowerCase()}`;
  let hash = 0x811c9dc5;
  for (const byte of Buffer.from(source, "utf8")) {
    hash ^= byte;
    hash = Math.imul(hash, 0x01000193) >>> 0;
  }
  return hash || 1;
}

function readImgEntries(buffer, fingerprints = false) {
  if (buffer.length < 8 || buffer.toString("ascii", 0, 4) !== "VER2") {
    throw new Error("player.img must use the IMG v2 format (VER2)");
  }
  const count = buffer.readUInt32LE(4);
  if (count > 200_000 || 8 + count * 32 > buffer.length) throw new Error("Invalid or truncated IMG index");
  const result = new Map();
  for (let index = 0; index < count; index += 1) {
    const entry = 8 + index * 32;
    const rawName = buffer.subarray(entry + 8, entry + 32);
    const zero = rawName.indexOf(0);
    const name = rawName.subarray(0, zero === -1 ? rawName.length : zero).toString("ascii").toLowerCase();
    if (!name) continue;
    if (!fingerprints || !name.endsWith(".dff")) {
      result.set(name, "");
      continue;
    }
    const offset = buffer.readUInt32LE(entry) * 2048;
    const allocated = buffer.readUInt32LE(entry + 4) * 2048;
    if (allocated < 12 || offset + allocated > buffer.length) continue;
    const chunkSize = buffer.readUInt32LE(offset + 4) + 12;
    const size = chunkSize >= 12 && chunkSize <= allocated ? chunkSize : allocated;
    result.set(name, sha256(buffer.subarray(offset, offset + size)));
  }
  return result;
}

function stripComment(line) {
  const positions = [line.indexOf("#"), line.indexOf(";"), line.indexOf("//")].filter((value) => value >= 0);
  return (positions.length ? line.slice(0, Math.min(...positions)) : line).trim();
}

function parseShopping(text) {
  const items = [];
  const hasSections = /^\s*section\s+/imu.test(text);
  const clothingSections = new Set(["clothes", "haircuts", "tattoos"]);
  let active = !hasSections;
  for (const original of text.split(/\r?\n/u)) {
    const line = stripComment(original);
    if (!line) continue;
    const section = /^section\s+(\S+)/iu.exec(line);
    if (section) {
      active = clothingSections.has(section[1].toLowerCase());
      continue;
    }
    if (/^end\b/iu.test(line)) {
      active = false;
      continue;
    }
    if (!active || /^(prices|shops|store|type)\b/iu.test(line)) continue;
    const tokens = line.split(/[\t ]+/u);
    if (tokens.length < 5) continue;
    const slot = Number.parseInt(tokens[3], 10);
    if (!Number.isInteger(slot) || slot < 0 || slot >= 18) continue;
    const texture = tokens[0].toLowerCase();
    const model = tokens[2].toLowerCase();
    if (!/^[a-z0-9_]+$/iu.test(texture) || !/^[a-z0-9_]+$/iu.test(model)) continue;
    const numeric = tokens.map((token) => Number.parseInt(token, 10)).filter(Number.isFinite);
    items.push({ texture, model, slot, gxt: tokens[1], price: Math.max(0, numeric.at(-1) ?? 0) });
  }
  if (!items.length) throw new Error("No clothing records were found in shopping.dat");
  return items;
}

const pawnEscape = (value) => String(value).replaceAll("\\", "\\\\").replaceAll('"', '\\"');
const pawnId = (value) => `0x${value.toString(16).padStart(8, "0")}`;

function makeBinary(items) {
  const chunks = [];
  const header = Buffer.alloc(10);
  header.write("HCAT", 0, "ascii"); // Kept for binary compatibility with protocol v1 clients.
  header.writeUInt16LE(1, 4);
  header.writeUInt32LE(items.length, 6);
  chunks.push(header);
  for (const item of items) {
    const texture = Buffer.from(item.texture, "ascii");
    const model = Buffer.from(item.model, "ascii");
    if (texture.length > 31 || model.length > 31) throw new Error(`Asset name is too long: ${item.texture}/${item.model}`);
    const record = Buffer.alloc(8);
    record.writeUInt32LE(item.id, 0);
    record.writeUInt8(item.slot, 4);
    record.writeUInt8(texture.length, 5);
    record.writeUInt8(model.length, 6);
    chunks.push(record, texture, model);
  }
  return Buffer.concat(chunks);
}

function makePawnInclude(items, version, defaults) {
  const lines = [
    `#define DRIP_CATALOG_VERSION \"${version}\"`,
    `#define DRIP_CATALOG_COUNT (${items.length})`,
    "#define DRIP_ITEM_NAME_MAX (48)",
    "#define DRIP_ITEM_CATEGORY_MAX (24)",
    "#define DRIP_ASSET_NAME_MAX (32)",
    "",
    "enum E_DRIP_CATALOG_ITEM",
    "{",
    "    DripItemID,",
    "    DripItemSlot,",
    "    DripItemPrice,",
    "    bool:DripItemEnabled,",
    "    DripItemName[DRIP_ITEM_NAME_MAX],",
    "    DripItemCategory[DRIP_ITEM_CATEGORY_MAX],",
    "    DripItemTexture[DRIP_ASSET_NAME_MAX],",
    "    DripItemModel[DRIP_ASSET_NAME_MAX]",
    "};",
    "",
    "new const DripCatalog[DRIP_CATALOG_COUNT][E_DRIP_CATALOG_ITEM] =",
    "{",
  ];
  items.forEach((item, index) => {
    const suffix = index + 1 === items.length ? "" : ",";
    lines.push(`    {${pawnId(item.id)}, ${item.slot}, ${item.price}, ${item.enabled ? "true" : "false"}, \"${pawnEscape(item.name)}\", \"${pawnEscape(item.category)}\", \"${pawnEscape(item.texture)}\", \"${pawnEscape(item.model)}\"}${suffix}`);
  });
  lines.push("};", "", "new const DripDefaultItem[DRIP_SLOT_COUNT] =", "{", `    ${defaults.map(pawnId).join(", ")}`, "};", "");
  return lines.join("\n");
}

const [playerImg, clothesDat, shoppingDat, basePlayerImg] = await Promise.all([
  readFile(playerImgPath),
  readFile(clothesDatPath),
  readFile(shoppingDatPath),
  basePlayerImgPath ? readFile(basePlayerImgPath) : Promise.resolve(null),
]);
const imgEntries = readImgEntries(playerImg);
const modifiedDffs = readImgEntries(playerImg, true);
const baseDffs = basePlayerImg ? readImgEntries(basePlayerImg, true) : new Map();
const unchangedBaseModels = new Set();
for (const [name, fingerprint] of modifiedDffs) {
  if (baseDffs.get(name) === fingerprint) unchangedBaseModels.add(name.slice(0, -4));
}

const overrides = overridesPath ? JSON.parse(await readFile(overridesPath, "utf8")) : {};
const seenIds = new Map();
const items = parseShopping(shoppingDat.toString("latin1")).map((item) => {
  if (!imgEntries.has(`${item.texture}.txd`)) throw new Error(`Missing texture in player.img: ${item.texture}.txd`);
  if (!imgEntries.has(`${item.model}.dff`)) throw new Error(`Missing model in player.img: ${item.model}.dff`);
  const id = stableId(item.slot, item.texture, item.model);
  if (seenIds.has(id)) throw new Error(`ID collision: ${seenIds.get(id)} and ${item.texture}/${item.model}`);
  seenIds.set(id, `${item.texture}/${item.model}`);
  const override = overrides[item.texture] ?? {};
  return {
    ...item,
    id,
    name: override.name ?? item.gxt,
    category: override.category ?? `slot-${item.slot}`,
    price: Number.isInteger(override.price) && override.price >= 0 ? override.price : item.price,
    enabled: typeof override.enabled === "boolean" ? override.enabled : !unchangedBaseModels.has(item.model),
  };
}).sort((left, right) => left.slot - right.slot || left.texture.localeCompare(right.texture));

const defaults = Array.from({ length: 18 }, () => 0);
const configuredDefaults = overrides.$defaults ?? {};
for (let slot = 0; slot < defaults.length; slot++) {
  const reference = configuredDefaults[String(slot)];
  const fallbackTexture = ["player_torso", "player_face", "player_legs", "foot"][slot];
  const normalized = String(reference ?? fallbackTexture ?? "").toLowerCase();
  const found = items.find((item) => item.slot === slot && (item.texture === normalized || String(item.id) === normalized));
  if (reference != null && !found) throw new Error(`Default item not found for slot ${slot}: ${reference}`);
  defaults[slot] = found?.id ?? 0;
}

const version = sha256(Buffer.concat([playerImg, clothesDat, shoppingDat])).slice(0, 16);
const catalog = makeBinary(items);
const seed = {
  schema: 1,
  protocol: 1,
  catalogVersion: version,
  files: {
    "models/player.img": sha256(playerImg),
    "data/clothes.dat": sha256(clothesDat),
    "data/shopping.dat": sha256(shoppingDat),
    "omp-drip/catalog.bin": sha256(catalog),
  },
};

await Promise.all([
  mkdir(path.join(outputRoot, "omp-drip"), { recursive: true }),
  mkdir(path.join(outputRoot, "generated"), { recursive: true }),
]);
await Promise.all([
  writeFile(path.join(outputRoot, "omp-drip", "catalog.bin"), catalog),
  writeFile(path.join(outputRoot, "generated", "omp-drip-catalog.inc"), makePawnInclude(items, version, defaults)),
  writeFile(path.join(outputRoot, "omp-drip", "catalog.seed.json"), `${JSON.stringify(seed, null, 2)}\n`),
]);

console.log(`omp-drip catalog ${version}: ${items.length} validated items (${unchangedBaseModels.size} unchanged base models disabled).`);
