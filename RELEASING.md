# Releasing uintDebugger

This repository uses a portable release layout and a GitHub Releases based update flow.

## 0. One-Time Repository Setup

Set these values in the root [CMakeLists.txt](./CMakeLists.txt) before your first public release:

- `UINTDEBUGGER_GITHUB_OWNER`
- `UINTDEBUGGER_GITHUB_REPOSITORY`
- `UINTDEBUGGER_UPDATE_MANIFEST_ASSET`

The current updater is unauthenticated. In practice that means the repository and its Releases must be public unless you extend the updater with token support.

## 1. Bump The Version

`uintDebugger` versioning is controlled from the root [CMakeLists.txt](./CMakeLists.txt).

Update:

- `project(uintDebugger VERSION x.y.z ...)`

That value feeds:

- application version metadata
- window titles and About dialog
- startup update comparison
- release manifest generation

## 2. Build The Release

```powershell
cmake -B Build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"
cmake --build Build --config Release
```

Expected output folder:

- `Release/`

## 3. Generate Update Assets

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\New-UpdateManifest.ps1 -ReleaseDir .\Release
```

This generates:

- `Release/github-release-assets/`
- `Release/github-release-assets/uintDebugger-update-manifest.json`
- `Release/github-release-assets/upload-release-assets.ps1`

The staging folder contains release assets renamed for GitHub Releases. Nested files such as `platforms/qwindows.dll` are flattened into asset names like `platforms__qwindows.dll`, while the manifest keeps the original in-app relative path.

## 4. Create The GitHub Release

Authenticate GitHub CLI first if needed:

```powershell
gh auth login
```

Recommended tag format:

- `vX.Y.Z`

With GitHub CLI:

```powershell
gh release create v0.2.0 --title "uintDebugger v0.2.0" --notes-file .\changelog.md
```

Then upload the generated assets:

```powershell
powershell -ExecutionPolicy Bypass -File .\Release\github-release-assets\upload-release-assets.ps1
```

If you also want a human-friendly portable archive, create and upload one separately. The updater does not consume the `.zip`; it consumes the manifest plus per-file assets.

Minimum assets required for updater compatibility:

- `uintDebugger-update-manifest.json`
- `uintDebugger.exe`
- `updater.exe`
- deployed Qt runtime DLLs
- deployed Qt plugin files
- any bundled data folders you want to patch in place

## 5. How The Updater Resolves Files

The startup checker queries:

- `https://api.github.com/repos/<owner>/<repo>/releases/latest`

The release must include:

- `uintDebugger-update-manifest.json`
- one release asset for each file listed in that manifest

Each manifest entry contains:

- `path`: destination path relative to the application root
- `asset`: GitHub Release asset name
- `size`: file size in bytes
- `sha256`: expected file hash

The updater downloads changed files into `updates/`, launches `updater.exe`, applies the files in place, removes `updates/`, and starts `uintDebugger.exe` again.

## 6. Installation Layout For Users

Users should end up with a portable folder containing at least:

- `uintDebugger.exe`
- `updater.exe`
- deployed Qt DLLs
- deployed Qt plugin subfolders such as `platforms/`, `styles/`, `imageformats/`, `iconengines/`

Do not separate `updater.exe` from the main application folder.

If you distribute a `.zip`, the extracted folder should preserve that exact layout.

## 7. Sanity Checklist Before Push

- `cmake --build Build --config Release`
- launch `Release/uintDebugger.exe`
- verify `Options -> Update` opens the updater and the startup check prompts correctly
- confirm the generated manifest contains the files you actually want to ship
- confirm the GitHub Release is public and contains every asset referenced by the manifest
