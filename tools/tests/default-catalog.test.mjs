import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";
import test from "node:test";

test("bundled vanilla catalog, source seed and Pawn include agree", async () => {
  const [binary, include, seedText, overridesText] = await Promise.all([
    readFile("catalog/default/catalog.bin"),
    readFile("include/omp-drip-catalog.inc", "utf8"),
    readFile("catalog/default/catalog.seed.json", "utf8"),
    readFile("catalog/default/overrides.json", "utf8"),
  ]);
  const seed = JSON.parse(seedText);
  assert.equal(seed.schema, 1);
  assert.equal(seed.protocol, 1);
  assert.equal(createHash("sha256").update(binary).digest("hex"), seed.files["omp-drip/catalog.bin"]);
  for (const file of ["models/player.img", "data/clothes.dat", "data/shopping.dat"]) {
    assert.match(seed.files[file], /^[a-f0-9]{64}$/u);
  }
  assert.ok(include.includes(`#define DRIP_CATALOG_VERSION "${seed.catalogVersion}"`));
  assert.match(include, /^#if defined _omp_drip_catalog_included/u);
  assert.equal(binary.toString("ascii", 0, 4), "HCAT");
  assert.equal(binary.readUInt16LE(4), 2);
  assert.equal(seed.catalogFormat, 2);
  const count = binary.readUInt32LE(6);
  assert.equal(count, 339);
  assert.ok(include.includes(`#define DRIP_CATALOG_COUNT (${count})`));

  const rows = [...include.matchAll(/\{0x([a-f0-9]{8}), (\d+), (\d+), (true|false), "([^"]*)", "([^"]*)", "([^"]*)", "([^"]*)"\}/gu)];
  assert.equal(rows.length, count);
  const ids = new Set();
  const tattooSlots = new Set();
  let tattooCount = 0;
  let offset = 10;
  for (const row of rows) {
    const id = binary.readUInt32LE(offset);
    const slot = binary[offset + 4];
    const textureLength = binary[offset + 5];
    const modelLength = binary[offset + 6];
    assert.equal(binary[offset + 7], 0);
    offset += 8;
    const texture = binary.toString("ascii", offset, offset + textureLength);
    offset += textureLength;
    const model = binary.toString("ascii", offset, offset + modelLength);
    offset += modelLength;
    assert.ok(id > 0 && !ids.has(id));
    ids.add(id);
    assert.ok(slot < 18);
    assert.ok(textureLength > 0 && textureLength <= 31 && modelLength <= 31);
    if (slot >= 4 && slot <= 12) {
      assert.equal(modelLength, 0);
      tattooCount++;
      tattooSlots.add(slot);
    } else {
      assert.ok(modelLength > 0);
    }
    assert.deepEqual([id, slot, texture, model], [Number.parseInt(row[1], 16), Number(row[2]), row[7], row[8]]);
    assert.equal(row[4], "true");
  }
  assert.equal(offset, binary.length);
  assert.equal(tattooCount, 48);
  assert.deepEqual([...tattooSlots], [4, 5, 6, 7, 8, 9, 10, 11, 12]);

  const defaultsText = include.match(/new const DripDefaultItem\[DRIP_SLOT_COUNT\] =\s*\{([^}]+)\}/u)?.[1];
  assert.ok(defaultsText);
  const defaults = defaultsText.trim().split(/\s*,\s*/u).map((value) => Number.parseInt(value, 16));
  assert.equal(defaults.length, 18);
  const overrides = JSON.parse(overridesText).$defaults;
  for (let slot = 0; slot < defaults.length; slot++) {
    const row = rows.find((row) => Number.parseInt(row[1], 16) === defaults[slot]);
    if (overrides[slot]) {
      assert.ok(row, `Missing default for slot ${slot}`);
      assert.equal(Number(row[2]), slot);
      assert.equal(row[7], overrides[slot]);
    } else {
      assert.equal(defaults[slot], 0);
    }
  }
});
