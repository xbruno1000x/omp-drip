Windows x86 binaries for the open.mp component and SA-MP 0.3.7-R3 client.

The client packager now validates source hashes when using the default catalog,
rejecting incompatible assets before writing the package. Custom assets require
an explicitly selected alternative catalog.

- Server ZIP: extract `components/omp-drip.dll` into your open.mp server and install the Pawn includes, including the default vanilla CJ catalog in `include/omp-drip-catalog.inc`.
- Client ZIP: contains `omp-drip.asi`, the matching `omp-drip/catalog.bin`, source hashes, client manifest, installation instructions and third-party license notices.
- Catalog ZIP: the default catalog's Pawn include, binary, source hashes and initial-outfit configuration.
- `SHA256SUMS.txt`: checksums for the attached binaries and archives.

The bundled catalog has 339 validated vanilla CJ items, including 48 tattoos.
Tattoo catalogs use binary format 2 and require the updated ASI; legacy format 1
catalogs remain supported by the new client. Pawn natives and network packets
are unchanged. The examples include a tattoo submenu with private previews,
confirmation and removal for each body region.
GTA assets are not included. The default client reads `models/player.img`,
`data/clothes.dat` and `data/shopping.dat` from the user's vanilla installation.
Extract the client ZIP into the game directory with an ASI loader. Remove a
previous `modloader/omp-drip` custom package when switching back to vanilla.
Developers can generate alternative catalogs and distribute custom client
packages as described in the [project README](https://github.com/xbruno1000x/omp-drip#1-use-the-default-catalog-or-generate-an-alternative).

The public component uses the unconfigured manifest (development mode).
To require exact client package hashes, generate your client package and then
rebuild the server component with its generated manifest before deployment.
