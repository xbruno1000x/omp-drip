# Default CJ catalog

The bundled catalog contains 339 validated records from the vanilla GTA San
Andreas files: 291 clothing, haircut and accessory items (including special
outfits), plus 48 tattoos across body slots 4 through 12.
Item names are the source GXT keys. The initial outfit is CJ's white vest,
original haircut, blue jeans and black sneakers; optional slots start empty.

`catalog.bin` is the client catalog. The matching Pawn data is checked in at
`include/omp-drip-catalog.inc` in the repository. Release catalog ZIPs contain
both, in `omp-drip/` and `include/` respectively. `catalog.seed.json` records
the SHA-256 hashes of the three source files and the binary catalog.
The catalog version also reflects the generated items, overrides and defaults.
GTA models, textures and data files are not redistributed.

Catalogs containing tattoos use binary format 2 and require the updated ASI.
The new client also reads legacy format 1 catalogs. Clothing-only catalogs
still use format 1. The network protocol and Pawn natives remain unchanged:
apply and preview a tattoo like any item, or use item ID 0 to remove it.

The server release ZIP includes the Pawn catalog. The client release ZIP
includes `omp-drip/catalog.bin`, its seed and a client manifest, alongside the
ASI. Extract the client ZIP into a compatible vanilla GTA installation with
an ASI loader. It reads the original files under `models/` and `data/`.
Remove a previous `modloader/omp-drip` custom package before switching back to
vanilla; a custom package takes precedence as a whole.

To regenerate this checked-in catalog from a local vanilla installation:

```powershell
./scripts/generate-default-catalog.ps1 -GtaDirectory "C:/path/to/GTA San Andreas"
npm test
```

The script reads the game files and updates the binary, seed and Pawn include
together. `overrides.json` defines the initial outfit. CI and releases use
these generated files without requiring GTA assets on the build machine.

Developers can generate an alternative catalog with
`tools/generate-catalog.mjs` and their own assets and overrides, then compile
their gamemode with that generated include ahead of the bundled include.
Pass the corresponding binary to `scripts/package-client.ps1 -Catalog` and
distribute the generated client package. To enforce package hashes, rebuild
the server component with that package's generated manifest.

The generator reads tattoo records from the `Tattoos` section of `shopping.dat`:

```text
section Tattoos
4weed 4WEED 4 - respect 1 sexy 2 50
end
```

The third field is the body slot (4..12), not a model name. Only the matching
texture dictionary (`4weed.txd` in this example) is required in `player.img`;
tattoos have no DFF model. The generated Pawn model field is empty. Custom
tattoos support the same name, category, price and enabled overrides as clothes.
