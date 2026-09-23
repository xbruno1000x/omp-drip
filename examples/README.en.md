# Default CJ example (English)

`cj-default-en.pwn` is the English version of `cj-default.pwn`. Both are
standalone open.mp gamemodes with the same clothing catalog and behavior.
The English script translates menus, chat messages, commands, logs and comments.
Messages sent directly by the native omp-drip component remain in Portuguese.

Players spawn as CJ at Didier Sachs, interior 14, at
`204.16, -165.76, 1000.52`. The fitting room is at
`215.2920, -156.1625, 1000.5234`, facing `90.1563` degrees. While trying on
clothes, the player is frozen and the camera faces CJ from 3.2 meters away.
Leaving restores the previous position, interior, controls and normal camera.

## Build

From the repository root, using the compiler and includes available locally:

```powershell
./scripts/prepare-cj-example.ps1 `
  -Language en `
  -GtaDirectory "C:/Program Files (x86)/GTA RIP" `
  -PawnCompiler "$env:APPDATA/sampctl/pawn/openmultiplayer/compiler/v3.10.11/pawncc.exe" `
  -PawnInclude @("../GTA Torcidas/dependencies/omp-stdlib")
```

Adjust those paths for your machine. This generates the catalog and compiles
`build/cj-default/cj-default-en.amx`. The Portuguese version remains the default
when `-Language` is omitted. Both versions use `cj-default.overrides.json`
and the same generated catalog; no new client package is needed solely to
switch the gamemode language. Items in native menus use their texture names.

Copy the English AMX into the server's `gamemodes` directory and set
`pawn.main_scripts` to `["cj-default-en 1"]` in `config.json` before starting
that version. The server requires `omp-drip.dll` in `components`, and the game
requires the matching omp-drip client package. See the repository
[README](../README.md) for native builds, packaging and client requirements.

## Commands and menus

| Command | Action |
| --- | --- |
| `/clothes` | Enter the fitting room and open the native category menu. |
| `/confirm` | Apply the preview and synchronize the outfit with other players. |
| `/cancel` | Discard the preview and leave the fitting room. |
| `/cj` | Leave the fitting room and restore CJ's default outfit. |
| `/resync` | Leave the fitting room and request streamed appearances again. |

Categories include torso, hair, legs, shoes, necklaces, watches, glasses,
hats and special outfits. Each page contains up to eight items plus navigation
and the default/remove action, within the native menu's twelve-row limit.
Selecting an item opens a private preview with **Confirm outfit**,
**Try another item** and **Cancel and leave** options.

Confirmed outfits survive respawns but reset on reconnect. The demo does not
charge money or persist outfits to a database. The current generator does not
import tattoos from the original `shopping.dat` format.
