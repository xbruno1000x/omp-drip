# Adapting omp-drip to a custom player.img

The catalog generator combines `shopping.dat` records with assets that actually
exist in `player.img`. Generation stops when a referenced `.txd` or `.dff` is
missing, preventing a broken package from reaching players.

## Overrides

Overrides are keyed by the texture name from `shopping.dat`:

```json
{
  "tshirtwhite": {
    "name": "White T-shirt",
    "category": "Torso",
    "price": 120,
    "enabled": true
  },
  "cj_only_mesh": { "enabled": false },
  "$defaults": {
    "0": "player_torso",
    "1": "player_face",
    "2": "player_legs",
    "3": "foot"
  }
}
```

`enabled` controls catalog visibility for the gamemode. Every record remains in
the binary catalog so previously persisted IDs can still be decoded.

Structural slots 0..3 default to the standard CJ base textures when present.
Optional slots 4..17 default to zero, avoiding characters that spawn with one
tattoo or accessory from every category.

## Detecting incompatible stock CJ models

When a custom `player.img` replaces CJ but retains original clothing meshes,
pass an original archive through `--base-player-img`. Unchanged DFFs are then
disabled by default. Explicit `"enabled": true` overrides that decision for
assets tested with the replacement model.

```powershell
node tools/generate-catalog.mjs `
  --player-img "custom/player.img" `
  --base-player-img "original/player.img" `
  --clothes-dat "custom/clothes.dat" `
  --shopping-dat "custom/shopping.dat" `
  --overrides catalog-overrides.json
```

## Stable IDs

An item ID is an FNV-1a hash of slot, texture and model. Prices and display names
can change without invalidating persisted outfits. Renaming a texture or model
creates a new ID and requires a migration in the consumer gamemode.

## Safe rollout

1. Generate and review all enabled records.
2. Test torso, head, legs and shoes before optional slots.
3. Test previews repeatedly; malformed model combinations can crash GTA.
4. Package the exact tested assets.
5. Rebuild the server component after generating the manifest.
6. Deploy server and client as one version.
