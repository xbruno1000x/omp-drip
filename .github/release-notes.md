Windows x86 binaries for the open.mp component and SA-MP 0.3.7-R3 client.

- Server ZIP: extract `components/omp-drip.dll` into your open.mp server and install the Pawn includes.
- Client ZIP: contains `omp-drip.asi` only, plus installation instructions and third-party license notices.
- `SHA256SUMS.txt`: checksums for the attached binaries and archives.

GTA assets, a generated catalog and a package-specific manifest are not included.
Generate a catalog from your own `player.img`, `clothes.dat` and `shopping.dat`
and package it with the ASI as described in the [project Wiki](https://github.com/xbruno1000x/omp-drip/wiki).

The public component uses the unconfigured manifest (development mode).
To require exact client package hashes, generate your client package and then
rebuild the server component with its generated manifest before deployment.
