import assert from "node:assert/strict";
import { mkdtemp, readFile, writeFile } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import path from "node:path";
import test from "node:test";

const generator = path.resolve("tools/generate-catalog.mjs");

function img(names) {
  const output = Buffer.alloc(8 + names.length * 32);
  output.write("VER2", 0, "ascii");
  output.writeUInt32LE(names.length, 4);
  names.forEach((name, index) => output.write(name, 8 + index * 32 + 8, 24, "ascii"));
  return output;
}

async function fixture(names, shopping) {
  const root = await mkdtemp(path.join(tmpdir(), "omp-drip-catalog-"));
  await Promise.all([
    writeFile(path.join(root, "player.img"), img(names)),
    writeFile(path.join(root, "clothes.dat"), "rule\n"),
    writeFile(path.join(root, "shopping.dat"), shopping),
  ]);
  return root;
}

function run(root, extra = []) {
  return spawnSync(process.execPath, [generator,
    "--player-img", path.join(root, "player.img"),
    "--clothes-dat", path.join(root, "clothes.dat"),
    "--shopping-dat", path.join(root, "shopping.dat"),
    "--out", root,
    ...extra,
  ], { encoding: "utf8" });
}

test("generates a deterministic catalog when assets exist", async () => {
  const root = await fixture(["shirt.txd", "torso.dff"], "shirt SHIRT torso 0 respect 0 sexy 0 120\n");
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
  const catalog = await readFile(path.join(root, "omp-drip", "catalog.bin"));
  assert.equal(catalog.toString("ascii", 0, 4), "HCAT");
  assert.equal(catalog.readUInt32LE(6), 1);
});

test("only reads appearance sections", async () => {
  const root = await fixture(["shirt.txd", "torso.dff"], [
    "section CarMods",
    "bumper BUMP respect 0 sexy 0 100",
    "end",
    "section Clothes",
    "shirt SHIRT torso 0 respect 0 sexy 0 120",
    "end",
  ].join("\n"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
  const catalog = await readFile(path.join(root, "omp-drip", "catalog.bin"));
  assert.equal(catalog.readUInt32LE(6), 1);
});

test("rejects a model missing from player.img", async () => {
  const root = await fixture(["shirt.txd"], "shirt SHIRT torso 0 respect 0 sexy 0 120\n");
  const result = run(root);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /Missing model/u);
});

test("uses CJ structural defaults and leaves optional slots empty", async () => {
  const root = await fixture([
    "player_torso.txd", "torso.dff",
    "player_face.txd", "head.dff",
    "player_legs.txd", "legs.dff",
    "foot.txd", "feet.dff",
    "watch.txd", "watch.dff",
  ], [
    "player_torso TORSO torso 0 respect 0 sexy 0 0",
    "player_face HEAD head 1 respect 0 sexy 0 0",
    "player_legs LEGS legs 2 respect 0 sexy 0 0",
    "foot FEET feet 3 respect 0 sexy 0 0",
    "watch WATCH watch 14 respect 0 sexy 0 10",
  ].join("\n"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
  const include = await readFile(path.join(root, "generated", "omp-drip-catalog.inc"), "utf8");
  const values = include.match(/new const DripDefaultItem\[DRIP_SLOT_COUNT\][\s\S]*?\{\s*([^}]+)\}/u)?.[1];
  assert.ok(values);
  const slots = values.trim().split(/\s*,\s*/u);
  assert.ok(slots.slice(0, 4).every((value) => value !== "0x00000000"));
  assert.ok(slots.slice(4).every((value) => value === "0x00000000"));
});

test("applies names, prices and compatibility overrides", async () => {
  const root = await fixture(["shirt.txd", "torso.dff"], "shirt SHIRT torso 0 respect 0 sexy 0 120\n");
  const overrides = path.join(root, "overrides.json");
  await writeFile(overrides, JSON.stringify({ shirt: { name: "Custom shirt", price: 999, enabled: false } }));
  const result = run(root, ["--overrides", overrides]);
  assert.equal(result.status, 0, result.stderr);
  const include = await readFile(path.join(root, "generated", "omp-drip-catalog.inc"), "utf8");
  assert.match(include, /999, false, "Custom shirt"/u);
});

test("packages the expected client layout and generates a server manifest header", async () => {
  const root = await mkdtemp(path.join(tmpdir(), "omp-drip-package-"));
  const files = {
    "omp-drip.asi": "asi",
    "player.img": "img",
    "clothes.dat": "clothes",
    "shopping.dat": "shopping",
    "catalog.bin": "catalog",
  };
  await Promise.all(Object.entries(files).map(([name, value]) => writeFile(path.join(root, name), value)));
  const output = path.join(root, "dist");
  const header = path.join(root, "manifest.hpp");
  const result = spawnSync(process.execPath, [path.resolve("tools/package.mjs"),
    "--asi", path.join(root, "omp-drip.asi"),
    "--player-img", path.join(root, "player.img"),
    "--clothes-dat", path.join(root, "clothes.dat"),
    "--shopping-dat", path.join(root, "shopping.dat"),
    "--catalog", path.join(root, "catalog.bin"),
    "--manifest-header", header,
    "--out", output,
    "--version", "test",
  ], { encoding: "utf8" });
  assert.equal(result.status, 0, result.stderr);
  const manifest = JSON.parse(await readFile(path.join(output, "omp-drip", "manifest.json"), "utf8"));
  assert.equal(manifest.version, "test");
  assert.ok(manifest.files["modloader/omp-drip/models/player.img"]);
  assert.match(await readFile(header, "utf8"), /kConfigured = true/u);
});
