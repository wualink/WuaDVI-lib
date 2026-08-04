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

## [0.3.0] - 2026-08-04

### Added

- **Runtime resolution selection.** `begin(res)` takes the mode and
  `setResolution(res)` changes it: the choice is stored in NVS and the board
  restarts into it, which is also how the display engine changes mode. The
  stored mode wins over the `begin()` argument, so a sketch written as
  `begin()` follows whatever was last selected. `storedResolution()` and
  `clearStoredResolution()` read and forget it.
- `wua_resolution.h`: the mode table and every size derived from it, mirroring
  the table the display-engine firmware keeps on its side.

### Changed

- The mode is no longer a build flag. `SCREEN_W`/`SCREEN_H`,
  `WUADVI_COLOR_MONO`, `RECT_PAYLOAD_MAX` and friends became runtime
  accessors, and the mono/colour paths that were `#if`-selected are now
  ordinary branches.
- Static wire buffers are sized to the worst case across every mode, which
  costs about 17 KB of RAM — the price of picking the mode at runtime.

[0.3.0]: https://github.com/wualink/WuaDVI-lib/releases/tag/v0.3.0

## [0.2.0] - 2026-08-04

The library now drives the board: everything the reference firmware did is here,
behind an API a sketch can use.

### Added

- **`WuaDVI` board class (L0).** `begin()` releases the display engine's strap
  pins, starts the SPI bus, brings the RP2354B up (flashing it when its firmware
  differs) and starts LVGL; `loop()` pumps LVGL, streams what it redrew and
  watches the engine's health, rebuilding the pipeline if it resets — the
  application's LVGL objects survive that and are simply repainted.
- **Transport and engine management (L1)**: ROM UART boot flashing, the control
  link, the dirty-rectangle stream with its telemetry back-channel.
- **Widget primitives (L2)**: `wua_tile`, `wua_gauge`, `wua_meter`, `wua_clock`,
  `wua_label`, `wua_value_label` — all sized as percentages and resolved for the
  active resolution.
- **Theming.** Colours moved out of the primitives into a `wua_theme_t` the
  application supplies. `wua_theme_set()` **rejects** a palette that would be
  unusable in the monochrome modes, where every colour collapses to one bit by
  luminance — an invisible interface is caught at startup instead of after
  switching resolution.
- **Zero-configuration install.** Declaring only this library is enough: LVGL is
  pulled in as a declared dependency, and if the project supplies no `lv_conf.h`
  the library installs a validated default where LVGL looks for one. A project
  that provides its own keeps it — the library never overwrites an existing file.
- **Full RP2354B payload embedding**: the pinned release is downloaded,
  verified against its SHA-256, converted from UF2 to the flat flash image and
  emitted as a C array, with a guard that refuses an image missing any of the
  five display modes.
- **A default resolution.** A sketch that sets no `-DWUADVI_RES_*` gets
  `640x480x1`, the mode with the most scanout margin, so the library works out
  of the box.

### Known limitations

- **Arduino IDE builds do not work yet.** The IDE cannot run library build
  scripts, which is how the firmware payload is produced. Use PlatformIO.
- Resolution is still a build-time flag; runtime selection is planned for 0.3.0.

[0.2.0]: https://github.com/wualink/WuaDVI-lib/releases/tag/v0.2.0

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
