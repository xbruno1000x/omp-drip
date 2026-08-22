#!/usr/bin/env node

import { createHash } from "node:crypto";
import { cp, mkdir, readFile, writeFile } from "node:fs/promises";
import path from "node:path";

const args = new Map();
for (let index = 2; index < process.argv.length; index += 2) args.set(process.argv[index], process.argv[index + 1]);
const required = (name) => {
  const value = args.get(name);
  if (!value) throw new Error(`Missing required argument: ${name}`);
  return path.resolve(value);
};

const asi = required("--asi");
const playerImg = required("--player-img");
const clothesDat = required("--clothes-dat");
const shoppingDat = required("--shopping-dat");
const catalog = required("--catalog");
const output = path.resolve(args.get("--out") ?? "dist/omp-drip-client");
const generatedHeader = path.resolve(args.get("--manifest-header") ?? "src/generated/manifest.hpp");
const version = args.get("--version") ?? new Date().toISOString().slice(0, 10).replaceAll("-", ".");

const hashFile = async (file) => createHash("sha256").update(await readFile(file)).digest("hex");
const sources = [asi, playerImg, clothesDat, shoppingDat, catalog];
const hashes = await Promise.all(sources.map(hashFile));
const manifest = {
  schema: 1,
  protocol: 1,
  version,
  files: {
    "omp-drip.asi": hashes[0],
    "modloader/omp-drip/models/player.img": hashes[1],
    "modloader/omp-drip/data/clothes.dat": hashes[2],
    "modloader/omp-drip/data/shopping.dat": hashes[3],
    "omp-drip/catalog.bin": hashes[4],
  },
};

await Promise.all([
  mkdir(path.join(output, "omp-drip"), { recursive: true }),
  mkdir(path.join(output, "modloader", "omp-drip", "models"), { recursive: true }),
  mkdir(path.join(output, "modloader", "omp-drip", "data"), { recursive: true }),
]);
await Promise.all([
  cp(asi, path.join(output, "omp-drip.asi")),
  cp(playerImg, path.join(output, "modloader", "omp-drip", "models", "player.img")),
  cp(clothesDat, path.join(output, "modloader", "omp-drip", "data", "clothes.dat")),
  cp(shoppingDat, path.join(output, "modloader", "omp-drip", "data", "shopping.dat")),
  cp(catalog, path.join(output, "omp-drip", "catalog.bin")),
]);

const manifestText = `${JSON.stringify(manifest, null, 2)}\n`;
await writeFile(path.join(output, "omp-drip", "manifest.json"), manifestText);
const manifestHash = createHash("sha256").update(manifestText).digest("hex");
const toBytes = (hex) => [...Buffer.from(hex, "hex")]
  .map((value) => `0x${value.toString(16).padStart(2, "0")}`)
  .join(", ");
const header = `#pragma once

#include <array>
#include <cstdint>

namespace drip::manifest {
inline constexpr bool kConfigured = true;
inline constexpr std::array<std::uint8_t, 32> kManifestHash = { ${toBytes(manifestHash)} };
inline constexpr std::array<std::array<std::uint8_t, 32>, 5> kFileHashes = {{
${hashes.map((hash) => `    { ${toBytes(hash)} }`).join(",\n")}
}};
} // namespace drip::manifest
`;
await mkdir(path.dirname(generatedHeader), { recursive: true });
await writeFile(generatedHeader, header);

const installation = `OMP-DRIP CLIENT ${version}

1. Copy every file from this directory to the GTA San Andreas directory.
2. Keep the directory structure unchanged.
3. Disable other mods that hook CJ clothing functions.
4. Remove older copies of omp-drip.asi before starting the game.

This package is tied to the server build through SHA-256 hashes. After running
the packager, rebuild omp-drip.dll so the server embeds the new manifest.
`;
await writeFile(path.join(output, "INSTALL.txt"), installation, "utf8");
console.log(`Client package ${version} written to ${output}. Rebuild omp-drip-server to embed its manifest.`);
