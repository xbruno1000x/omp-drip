# Installing with sampctl (open.mp)

The package supports Windows x86 open.mp servers. Use sampctl 1.14.1 or newer
and set `"preset": "openmp"` in your gamemode's `pawn.json`.

## Install the component and includes

From your gamemode directory:

```powershell
sampctl install component://xbruno1000x/omp-drip@main
sampctl ensure
```

The `@main` reference is intentional: release `v1.0.0` predates `pawn.json`.
It reads the package metadata from the main branch and downloads the latest
published server ZIP. After a new release includes this manifest, you can
replace `@main` with `:vX.Y.Z` using that release's actual tag.

A consumer package can look like this:

```json
{
  "preset": "openmp",
  "entry": "gamemodes/main.pwn",
  "output": "gamemodes/main.amx",
  "dependencies": [
    "openmultiplayer/omp-stdlib",
    "component://xbruno1000x/omp-drip@main"
  ]
}
```

The `component://` scheme installs `components/omp-drip.dll`; the dependency's
`include/` directory supplies `#include <omp-drip>` to the Pawn compiler.
Do not add this DLL to SA-MP's `plugins` list or open.mp's `pawn.legacy_plugins`.
See sampctl's [component dependency documentation](https://github.com/Southclaws/sampctl/blob/master/docs/dependency-schemes.md).

The resource uses `plugins` as the field name because that is sampctl's archive
schema, even for components. It selects only the Windows x86 **server** ZIP
and extracts `components/omp-drip.dll`. No Linux or x64 binary is advertised.

## Use your generated catalog

Generate the catalog from your own GTA files as described in the root README.
Copy `build/catalog/generated/omp-drip-catalog.inc` into your gamemode project's
`include/` directory, then explicitly include it from your script so the
placeholder in this dependency cannot be selected accidentally. For a source
file at `gamemodes/main.pwn`:

```pawn
#include <open.mp>
#include <omp-drip>
#include "../include/omp-drip-catalog.inc"
```

Then compile and run your own gamemode:

```powershell
sampctl build
sampctl run
```

sampctl installs the server dependency. Players still need the matching ASI,
catalog and GTA asset package. The public component uses the unconfigured
development manifest; rebuild it with your generated manifest to enforce
package hashes. Keep that custom DLL deployment separate from `sampctl ensure`,
which can reinstall the public release binary.

## Check the package from a source checkout

```powershell
sampctl ensure
sampctl build
```

This compiles `tests/pawn-api.pwn` into `build/pawn-api.amx`, checking the public
API with the open.mp compiler and includes. It is a compile check, not the
playable CJ demo. The playable examples and their catalog setup remain in
[`examples/README.md`](../examples/README.md).
