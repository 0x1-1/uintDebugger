# uintDebugger

`uintDebugger` is a Windows debugger for `x86`, `x64`, and `WOW64` targets. This repository is the Qt 6 + CMake continuation of the legacy codebase and keeps the original debugger workflow while moving the project to a modern Windows toolchain.

## Status

- Main application: `qtUintDebugger/`
- Updater stack: `uintDebuggerUpdater/`
- Build system: CMake + Qt 6
- Distribution model: portable release folder
- Update channel: GitHub Releases + manifest-driven file updates
- Version source of truth: root `CMakeLists.txt`

## Features

- User mode debugging for `x86`, `x64`, and `WOW64`
- Software, hardware, and memory breakpoints
- Step in, step over, step out, and run to user code
- Trace window, stack view, call stack view, PE inspection, strings, heap, handles, windows, and memory views
- Bookmark, comment, patch, and project file support
- Startup version check against the latest GitHub release
- Manual updater window under `Options -> Update`

## Requirements

- Windows 10/11
- Qt `6.10.2` MSVC 2022 x64
- CMake `3.25+`
- Visual Studio 2022 Community or Build Tools

## Build

```powershell
cmake -B Build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
cmake --build Build --config Release
```

The resulting portable binaries are placed in `Release/`.

## Install

`uintDebugger` currently ships as a portable directory, not as an MSI or installer.

1. Build or download the `Release` output.
2. Keep `uintDebugger.exe`, `updater.exe`, Qt DLLs, and plugin folders in the same deployed layout.
3. Launch `uintDebugger.exe` from that folder.

`updater.exe` must remain next to `uintDebugger.exe`. The in-app updater depends on that layout.

Recommended portable layout:

```text
uintDebugger/
  uintDebugger.exe
  updater.exe
  Qt6Core.dll
  Qt6Gui.dll
  Qt6Network.dll
  Qt6Svg.dll
  Qt6Widgets.dll
  platforms/
  styles/
  iconengines/
  imageformats/
  tls/
  networkinformation/
```

## Updating

- On startup, the application checks the latest GitHub Release in the configured repository.
- If a newer version exists, the UI prompts to open the updater.
- Manual checks are available through `Options -> Update`.
- File updates are driven by the release asset `uintDebugger-update-manifest.json`.
- GitHub owner, repository, and manifest asset names are configured in the root [CMakeLists.txt](./CMakeLists.txt).
- The current updater assumes a public GitHub repository. If you want private releases, the updater needs authenticated API and asset downloads.

Release generation and publishing are documented in [RELEASING.md](./RELEASING.md).

## Shipping An Update

1. Bump `project(uintDebugger VERSION ...)` in the root [CMakeLists.txt](./CMakeLists.txt).
2. Build `Release`.
3. Run [tools/New-UpdateManifest.ps1](./tools/New-UpdateManifest.ps1) to generate release assets and the update manifest.
4. Create a GitHub Release with tag format `vX.Y.Z`.
5. Upload every generated asset from `Release/github-release-assets/`.

The application startup check compares the current version against the latest GitHub Release and offers to open the updater when a newer version exists.

## Repository Layout

- `qtUintDebugger/`: main debugger sources, UI, resources, and CMake target
- `uintDebuggerUpdater/`: updater UI and file-application bootstrap executable
- `tools/`: helper scripts for release packaging and updater manifests

## Documentation

- Release workflow: [RELEASING.md](./RELEASING.md)
- Updater architecture: [uintDebuggerUpdater/README.md](./uintDebuggerUpdater/README.md)
- Changelog: [changelog.md](./changelog.md)
- Third-party notices: [LICENSES.md](./LICENSES.md)

## License

The project source code is licensed under `GPL-3.0-or-later`. See [LICENSE](./LICENSE).
