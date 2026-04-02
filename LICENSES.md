# License Notes

## Project License

`uintDebugger` source code is distributed under `GPL-3.0-or-later`.

Primary license file:

- [LICENSE](./LICENSE)

## Bundled / Referenced Components

- `Qt 6`
  - The project links against Qt and deploys Qt runtime binaries in release builds.
  - Your redistribution obligations depend on the Qt license you use.

- `BeaEngine`
  - The bundled BeaEngine headers indicate `LGPL-3.0-or-later`.
  - See files under [qtUintDebugger/BeaEngine](./qtUintDebugger/BeaEngine).

- `DbgHelp`
  - The project uses `dbghelp.h` and links against a bundled import library for the Windows debugging APIs.
  - Redistribution remains subject to Microsoft's terms for the corresponding SDK/runtime components.

## Practical Release Notes

- The repository source stays under `GPL-3.0-or-later`.
- Release folders may also contain third-party runtime files such as Qt platform plugins and Windows debugging components.
- Before publishing binaries, review the licenses that apply to the exact runtime files you ship.

This file is a practical summary for repository consumers, not legal advice.
