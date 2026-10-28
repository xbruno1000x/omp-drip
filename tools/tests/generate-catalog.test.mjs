import assert from "node:assert/strict";
import { mkdtemp, mkdir, readFile, writeFile, access } from "node:fs/promises";
import { createHash } from "node:crypto";
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
  assert.equal(catalog.readUInt16LE(4), 1);
  assert.equal(catalog.readUInt32LE(6), 1);
  const include = await readFile(path.join(root, "generated", "omp-drip-catalog.inc"), "utf8");
  assert.match(include, /^#if defined _omp_drip_catalog_included\n    #endinput\n#endif/u);
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

test("imports vanilla tattoo records without requiring a DFF and preserves clothing IDs", async () => {
  const root = await fixture(["shirt.txd", "torso.dff", "4weed.txd", "12angels.txd"], [
    "section prices",
    "section Clothes",
    "shirt SHIRT torso 0 respect 0 sexy 0 120",
    "end",
    "section Tattoos",
    "4weed 4WEED 4 - respect 1 sexy 2 50",
    "12angels 12ANGEL 12 - respect 3 sexy 3 450",
    "end",
    "end",
    "section shops",
    "section Tattoos",
    "type Tattoos",
    "item 4weed",
    "end",
    "end",
  ].join("\n"));
  const result = run(root);
  assert.equal(result.status, 0, result.stderr);
  const binary = await readFile(path.join(root, "omp-drip", "catalog.bin"));
  assert.equal(binary.readUInt16LE(4), 2);
  assert.equal(binary.readUInt32LE(6), 3);
  const include = await readFile(path.join(root, "generated", "omp-drip-catalog.inc"), "utf8");
  assert.match(include, /, 4, 50, true, "4WEED", "slot-4", "4weed", ""/u);
  assert.match(include, /, 12, 450, true, "12ANGEL", "slot-12", "12angels", ""/u);
  const legacy = await fixture(["shirt.txd", "torso.dff"], "shirt SHIRT torso 0 respect 0 sexy 0 120");
  assert.equal(run(legacy).status, 0);
  const oldBinary = await readFile(path.join(legacy, "omp-drip", "catalog.bin"));
  assert.equal(binary.readUInt32LE(10), oldBinary.readUInt32LE(10));
});

test("rejects a missing tattoo texture", async () => {
  const root = await fixture([], "section Tattoos\n4weed 4WEED 4 - respect 1 sexy 2 50\nend");
  const result = run(root);
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /Missing texture.*4weed\.txd/u);
});

test("rejects tattoo records outside slots 4..12", async () => {
  for (const slot of ["0", "3", "13", "18", "4oops"]) {
    const root = await fixture(["4weed.txd"], `section Tattoos\n4weed 4WEED ${slot} - respect 1 sexy 2 50\nend`);
    const result = run(root);
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /Invalid tattoo slot/u);
  }
});

test("tattoos support overrides and change catalog version without changing IDs", async () => {
  const root = await fixture(["4weed.txd"], "section Tattoos\n4weed 4WEED 4 - respect 1 sexy 2 50\nend");
  assert.equal(run(root).status, 0);
  const originalBinary = await readFile(path.join(root, "omp-drip", "catalog.bin"));
  const originalSeed = JSON.parse(await readFile(path.join(root, "omp-drip", "catalog.seed.json"), "utf8"));
  const overrides = path.join(root, "overrides.json");
  await writeFile(overrides, JSON.stringify({ "4weed": { name: "Custom tattoo", category: "Tattoos", price: 99, enabled: false } }));
  const result = run(root, ["--overrides", overrides]);
  assert.equal(result.status, 0, result.stderr);
  const updatedSeed = JSON.parse(await readFile(path.join(root, "omp-drip", "catalog.seed.json"), "utf8"));
  assert.notEqual(updatedSeed.catalogVersion, originalSeed.catalogVersion);
  assert.deepEqual(await readFile(path.join(root, "omp-drip", "catalog.bin")), originalBinary);
  const include = await readFile(path.join(root, "generated", "omp-drip-catalog.inc"), "utf8");
  assert.match(include, /, 4, 99, false, "Custom tattoo", "Tattoos", "4weed", ""/u);
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

test("client packager rejects incompatible sources for the default catalog before writing output", async () => {
  const root = await mkdtemp(path.join(tmpdir(), "omp-drip-default-package-"));
  for (const file of ["omp-drip.asi", "player.img", "clothes.dat", "shopping.dat"]) {
    await writeFile(path.join(root, file), file);
  }
  const result = spawnSync(process.execPath, [path.resolve("tools/package.mjs"),
    "--asi", path.join(root, "omp-drip.asi"),
    "--player-img", path.join(root, "player.img"),
    "--clothes-dat", path.join(root, "clothes.dat"),
    "--shopping-dat", path.join(root, "shopping.dat"),
    "--manifest-header", path.join(root, "manifest.hpp"),
    "--out", path.join(root, "dist"),
  ], { encoding: "utf8" });
  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /Default catalog source mismatch: models\/player.img/u);
  await assert.rejects(access(path.join(root, "dist")));
  await assert.rejects(access(path.join(root, "manifest.hpp")));
});

test("default catalog packaging checks every seed hash, including when selected explicitly", async () => {
  const root = await mkdtemp(path.join(tmpdir(), "omp-drip-default-seed-"));
  await mkdir(path.join(root, "catalog", "default"), { recursive: true });
  const seed = { files: {} };
  for (const file of ["models/player.img", "data/clothes.dat", "data/shopping.dat", "omp-drip/catalog.bin"]) {
    const content = file;
    const destination = file === "omp-drip/catalog.bin" ? "catalog/default/catalog.bin" : file;
    await mkdir(path.dirname(path.join(root, destination)), { recursive: true });
    await writeFile(path.join(root, destination), content);
    seed.files[file] = createHash("sha256").update(content).digest("hex");
  }
  await writeFile(path.join(root, "asi"), "asi");
  const seedPath = path.join(root, "catalog/default/catalog.seed.json");
  const command = [path.resolve("tools/package.mjs"), "--asi", "asi", "--player-img", "models/player.img",
    "--clothes-dat", "data/clothes.dat", "--shopping-dat", "data/shopping.dat",
    "--manifest-header", "manifest.hpp", "--out", "dist"];
  await writeFile(seedPath, JSON.stringify(seed));
  const good = spawnSync(process.execPath, command, { cwd: root, encoding: "utf8" });
  assert.equal(good.status, 0, good.stderr);
  for (const file of Object.keys(seed.files)) {
    const changed = structuredClone(seed);
    changed.files[file] = "0".repeat(64);
    await writeFile(seedPath, JSON.stringify(changed));
    const bad = spawnSync(process.execPath, [...command, "--catalog", "catalog/default/catalog.bin"], { cwd: root, encoding: "utf8" });
    assert.notEqual(bad.status, 0);
    assert.ok(bad.stderr.includes(`Default catalog source mismatch: ${file}`));
  }
});
