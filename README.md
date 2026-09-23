# omp-drip

`omp-drip` is an open.mp component plus a SA-MP client plugin that synchronizes
CJ-style clothing by body slot. A server can build a catalog from its own
`player.img`, preview individual pieces, persist outfits in its gamemode and
synchronize the resulting appearance to streamed players.

The repository contains the reusable native layer. Shop menus, prices, payment,
inventory ownership and database persistence intentionally remain in the
consumer gamemode.

Documentation is maintained in the [project Wiki](https://github.com/xbruno1000x/omp-drip/wiki).

## Features

- 18 GTA San Andreas clothing slots;
- full appearance and single-item preview synchronization;
- stable item IDs derived from slot, texture and model names;
- catalog validation against a custom `player.img`;
- optional detection of unchanged CJ models that may be incompatible with a replacement player model;
- SHA-256 validation of the distributed client package;
- duplicate-client and clothing-hook conflict detection;
- Pawn natives and ready/rejected callbacks;
- native protocol, catalog and hook-conflict tests.

## Compatibility

The current client implementation targets GTA San Andreas 1.0 US, SA-MP
0.3.7-R3 and Windows x86 with an ASI loader and Mod Loader. The open.mp server
component is also built as Windows x86.

Supporting another game or SA-MP revision requires adding and validating a
separate address/layout profile in `src/client/client.cpp`; do not reuse the
current addresses blindly.

## Prerequisites

- Node.js 20 or newer;
- CMake 3.19 or newer;
- Visual Studio 2022 C++ build tools with the Win32 toolchain;
- Git;
- your legally obtained `player.img`, `clothes.dat` and `shopping.dat` files.

No GTA assets are included in this repository.

## 1. Generate a catalog for your player.img

```powershell
node tools/generate-catalog.mjs `
  --player-img "C:\path\to\player.img" `
  --clothes-dat "C:\path\to\clothes.dat" `
  --shopping-dat "C:\path\to\shopping.dat" `
  --overrides catalog-overrides.json `
  --out build/catalog
```

This writes `build/catalog/omp-drip/catalog.bin`, a generated Pawn include and
a source-hash seed. See [the custom player.img guide](https://github.com/xbruno1000x/omp-drip/wiki/Custom-player.img)
for compatibility and default-item configuration.

## 2. Build the native binaries

```powershell
./scripts/bootstrap-native.ps1
./scripts/build.ps1 -Configuration Release
```

The outputs are `omp-drip.dll` (open.mp component) and `omp-drip.asi` (SA-MP
client plugin) under the CMake configuration directory.

## 3. Package the client

```powershell
./scripts/package-client.ps1 `
  -Asi "build/native/Release/omp-drip.asi" `
  -PlayerImg "C:\path\to\player.img" `
  -ClothesDat "C:\path\to\clothes.dat" `
  -ShoppingDat "C:\path\to\shopping.dat" `
  -Version "1.0.0"
```

The packager creates `dist/omp-drip-client` and regenerates
`src/generated/manifest.hpp`. Rebuild the server component afterward so it
embeds the package hashes. Without a generated manifest the component runs in
development mode and does not require the handshake.

## 4. Integrate with a gamemode

For an open.mp gamemode managed by sampctl 1.14.1+, set `"preset": "openmp"`
in its `pawn.json` and install the Windows x86 component and includes:

```powershell
sampctl install component://xbruno1000x/omp-drip@main
sampctl ensure
```

Use `@main` until a release newer than `v1.0.0` includes the new package
manifest. See [the sampctl guide](https://github.com/xbruno1000x/omp-drip/wiki/Installing-with-sampctl) for version pinning,
catalog setup and a consumer `pawn.json` example.

For manual installation:

1. Copy `omp-drip.dll` to the open.mp `components` directory.
2. Install `include/omp-drip.inc` and the generated catalog include.
3. Implement ownership, persistence and shop logic using the API.
4. Distribute the complete generated client package to every player.

Start with [`examples/basic.pwn`](examples/basic.pwn) and
[the Pawn API reference](https://github.com/xbruno1000x/omp-drip/wiki/Pawn-API).

For a standalone CJ demo with spawn, clothing menus, private previews and
confirmation commands, see [`examples/cj-default.pwn`](examples/cj-default.pwn)
and the [setup instructions in Portuguese](examples/README.md). Generate its
catalog with `scripts/prepare-cj-example.ps1` using your GTA data files.
An [English script and setup guide](examples/README.en.md) are also available;
pass `-Language en` to the preparation script to compile that version.

## CI and releases

GitHub Actions builds and tests Windows x86 on pushes to `main` and pull
requests. Download the binaries from the workflow artifacts, or publish a
version through **Actions > Create Release** with a version such as `1.0.0`.
Pushing a tag such as `v1.0.0` also builds and publishes a release.

Releases include the component DLL, client ASI, Pawn include, installation ZIPs
and SHA-256 checksums. GTA files and generated catalogs are not included.
See [the release guide](https://github.com/xbruno1000x/omp-drip/wiki/Builds-and-releases) for setup and manifest requirements.

## Tests

```powershell
npm test
./scripts/build.ps1 -Configuration Release
```

## Security model

The handshake verifies exact SHA-256 hashes for the ASI, `player.img`, data
files and binary catalog. This prevents accidental mixed versions and common
package modifications; it is not a general-purpose anti-cheat.
