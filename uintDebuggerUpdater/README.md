# uintDebugger Updater

The updater stack has two parts:

## 1. In-App Update UI

Location:

- [UpdaterWidget](./UpdaterWidget)

Responsibilities:

- query the latest GitHub Release
- compare the release version with the current application version
- download `uintDebugger-update-manifest.json`
- compare local files using `SHA-256`
- download only changed files into `updates/`
- launch `updater.exe` to apply them
- prompt on application startup when the latest public release is newer than the current version

## 2. Bootstrap Updater Executable

Location:

- [main.cpp](./main.cpp)

Responsibilities:

- apply downloaded files from `updates/`
- handle `updater.exe` self-update safely through `update_tmp.exe`
- relaunch `uintDebugger.exe`

## Release Contract

The latest GitHub Release must contain:

- `uintDebugger-update-manifest.json`
- one asset per file referenced by the manifest

GitHub asset redirects are expected. The downloader preserves the intended destination path while following them.

The manifest maps GitHub asset names back to their real destination paths inside the portable application directory.

## Publishing

Use the release workflow in [../RELEASING.md](../RELEASING.md).
