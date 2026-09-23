# GitHub Actions builds and releases

The workflows follow the manual **Create Release** pattern used by omp-cef.
Only Windows x86 is currently supported by omp-drip.

## Continuous integration

**Build and Test** runs on pushes to `main`, pull requests and manual runs.
It runs the Node.js tests, builds the DLL and ASI in Release configuration,
runs native tests with assertions enabled, checks PE architecture, and uploads
downloadable release assets as a workflow artifact for 14 days.

The build uses a Windows 2022 runner, Node.js 24 and the existing native
bootstrap/build scripts. It does not need GTA files or additional secrets.

## Publish a version

Once these files are pushed to the repository's default branch:

1. Open **Actions > Create Release > Run workflow**.
2. Select the branch or tag to build and enter a version such as `1.0.0`
   without the `v` prefix.
3. Wait for validation, build, tests and packaging. The workflow publishes
   release `v1.0.0` and attaches the tested assets.

Alternatively, push a stable version tag:

```sh
git tag v1.0.0
git push origin v1.0.0
```

Both paths build the selected revision and publish those exact artifacts.
Manual runs create the tag at the built commit if it does not exist. An
existing tag must match that commit. Versions must use `x.y.z` with no leading
zeroes; published versions are never overwritten. Failed uploads leave a draft
that can be retried using the same revision and version.

The release job alone has `contents: write`; build jobs and pull requests have
read-only repository access. GitHub's automatic `GITHUB_TOKEN` is used, so no
personal token is needed. Repository or organization policies must allow
Actions to publish releases. The workflow generates release notes and publishes
only after asset upload finishes, using the [GitHub CLI release commands](https://cli.github.com/manual/gh_release_create).

## Packages and manifest

The release contains versioned Windows x86 server/client ZIPs, the DLL, ASI,
Pawn include and a checksum file. The server archive has the open.mp
`components/` layout. These are generic binaries: GTA assets and local
catalogs are never part of the CI build or release assets.

The checked-in `src/generated/manifest.hpp` must remain unconfigured for public
builds. That component runs in development mode. For an enforced client package,
generate your catalog, package the exact client files and rebuild the server
with the generated manifest as described in the [README](../README.md).

To create the same release packages locally after building:

```powershell
./scripts/bootstrap-native.ps1
./scripts/build.ps1 -Configuration Release -Version 1.0.0
./scripts/package-release.ps1 -Version 1.0.0
```

Use the release workflow to select the component's reported version; changing
only an archive's filename does not change the version inside the component.
