# Changelog

All notable changes to **WuaDVI-lib** are recorded here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
Because two projects depend on this library, the version is a contract: a
**MAJOR** bump means a breaking API change, **MINOR** adds functionality
compatibly, **PATCH** is a compatible fix.

The version is declared in `library.json`, `library.properties` and the git tag;
CI verifies all three agree.

Each pull request should add a line under **[Unreleased]**; on release those
lines move under a new version heading.

## [Unreleased]

_Nothing yet._

## [0.1.0] - 2026-07-28

Initial skeleton: packaging and RP2354B firmware management. The board API and
widget primitives are extracted from the reference firmware in 0.2.0 — see the
README roadmap.

### Added

- **Dual toolchain packaging**: `library.json` (PlatformIO) and
  `library.properties` + `keywords.txt` (Arduino IDE), so the library installs
  in either environment.
- **Library-owned RP2354B firmware pinning.** `assets/rp-firmware/VERSION` and
  `SHA256` pin an exact [WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite)
  release; the binary itself is not vendored. Pinning by tag alone is not enough
  because a tag can be moved, so the hash is what identifies the artifact.
- **Build hook** (`scripts/library_build.py`, declared as the library's
  `extraScript`) that generates the firmware payload header and places it on the
  library's include path — the consuming application only ever includes
  `<WuaDVI.h>` and never needs to know the RP2354B exists.
- `HelloWuaDVI` example, reference LVGL configuration (`extras/`), MIT licence,
  contribution guide, clang-format house style, CI and issue/PR templates.

[Unreleased]: https://github.com/wualink/WuaDVI-lib/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/wualink/WuaDVI-lib/releases/tag/v0.1.0
