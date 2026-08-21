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

### Added

- **`wua_clear()` and `wua_settle()`** — the two calls a screen rebuilt at
  runtime needs. `wua_clear()` empties a container and releases the composite
  handles whose widgets went with it; `wua_settle()` resolves pending layouts
  between build passes.

  `wua_settle()` exists because of a failure that only appears once a screen
  can change shape: `wua_gauge()` measures its parent as it is created, and a
  parent sized in percent has no real size until its siblings exist. A
  one-pass build therefore produced a wrong diameter, a font derived from that
  diameter, and a needle length derived from it again — so a gauge was correct
  in the layout it was written for and wrong in every other.

- **Everything a sketch needs besides widgets**, so an application can be
  written end to end without naming LVGL: `wua_screen()`, `wua_header()` and
  `wua_grid()` for the layout; `wua_color()`; a setter per widget
  (`wua_slider_set`, `wua_led_set`, `wua_roller_select`, …); `wua_timer()` for
  state that steps and `wua_sweep()` for values that should look continuous.
  The `wua_obj_t` and `wua_color_t` typedefs complete the vocabulary.

  Creating widgets was never the whole job. With construction wrapped but
  values, timers and layout left to LVGL, a sketch still had to know which
  calls survive the monochrome modes — which is the knowledge the primitives
  exist to hold. A setter has the shape `wua_sweep()` wants, so
  `wua_sweep(slider, wua_slider_set, 0, 100, 2600)` replaces a hand-written
  animation callback.

- **A primitive for every standard widget**: `wua_button`, `wua_checkbox`,
  `wua_switch`, `wua_led`, `wua_slider`, `wua_bar`, `wua_arc`, `wua_spinner`,
  `wua_roller`, `wua_dropdown`, plus `wua_fit()` for parent-relative sizing.
  Each takes the parent, sizes in percentages and its values, and returns the
  plain LVGL object so behaviour stays ordinary LVGL.

  Found on hardware, and worth recording because it is invisible everywhere
  else: LVGL styles widget parts in shades of one hue, and the monochrome modes
  reduce each pixel to one bit **by luminance**, so a slider's track and its
  indicator — or a switch's track and its knob — land on the same side of the
  threshold and the widget renders as a flat rectangle. An `lv_led` that is off
  disappears entirely. The same sketch looked correct at 400x240 and unusable
  at 640x480. The primitives rebuild each widget from outlines and full-white
  indicators, which is geometry rather than shade, and geometry survives
  thresholding. They also resolve sizes and fonts from the parent, which fixes
  the clipping seen on the arc, roller and dropdown at 400x240.

- `wua_column()` and `wua_row()` — containers that carry a layout. Found on
  hardware: `wua_container()` has none, so its children all land at the same
  position and silently overlap, and an unlaid-out container also takes room
  from a sibling meant to grow. Both primitives size themselves sensibly, so
  the common case stops being a trap.

### Fixed

- **Composite sweeps no longer outlive their widgets.** The animations
  `wua_gauge_sweep()` and `wua_meter_sweep()` start carry the composite handle
  as their variable, not an LVGL object, so `lv_obj_clean()` did not cancel
  them: after a rebuild a tick could still dereference deleted children, or a
  freshly zeroed slot. Slots are now released from what LVGL reports as alive,
  and the animation is cancelled as the slot is freed.

- **A partial clear no longer invalidates unrelated handles.** The pools were
  emptied wholesale by `wua_ui_init()`, which callers were told to run after
  `wua_clear()`. Clearing a subtree then handed a surviving widget's slot to
  the next one created, and the application's handle drove the wrong widget.
  Slots are freed individually and **in place** — compacting the pool would
  move a survivor's address, which is the same defect wearing a tidier shape.

- **The gauge's stacking decision weighs the whole row.** It compared the dial
  and padding against the available width, ignoring the readout beside them —
  the very thing that gets pushed out. It is also measured after the 24 px
  minimum-diameter clamp, which can undo a fit the loop had just achieved.

- **`wua_gauge()` no longer pushes its readout out of the panel.** The gauge is
  a row — the disc with the number beside it — but the diameter was taken from
  the smaller side of the parent without deducting what the readout needs
  horizontally. A wide enough panel absorbed that and a narrow one did not, so
  the same gauge was correct at 1280x720 and overflowing at every other
  resolution. The two are now settled together: the readout's font is derived
  from the diameter, so shrinking the disc shrinks the number and frees more
  width than it costs.

- **A settled screen is no longer mistaken for a dead display engine.**
  Telemetry rides on the pixel packets, so a screen with nothing animating on
  it sends nothing and hears nothing back. Both ages were compared against the
  same six-second window, and since telemetry is always at least as old as the
  last packet they crossed it together: a healthy board rebuilt its pipeline
  every six seconds and read as a boot loop. The signal is the **gap** between
  them — how long packets have been going out unanswered. A recovery now also
  logs the numbers that separate the engine dying from this side deciding it
  did.

- **Bring-up no longer stalls when no serial terminal is open.** A USB-CDC port
  that is plugged in counts as *connected* even with nobody reading it, so the
  TX buffer fills and the write blocks — the board sat on the display engine's
  splash until a monitor was opened.

  The timeout is now 10 ms, and specifically **not** zero, which is the value
  that looks like "never wait" and is the only one that waits forever:
  `HWCDC::write()` counts down `tries = tx_timeout_ms` while no data moves, so
  zero underflows a `uint32_t` on the first pass and the give-up loop runs for
  about 49 days. Every bundled sketch was carrying that zero and now carries 10.

- **`LV_MEM_SIZE` raised from 96 KB to 128 KB.** LVGL's pool is a static array
  and widgets are sized in pixels, so a screen costs about four times as much
  at 1280×720 as at 640×480; a widget-rich screen exhausted it. Exhaustion is
  not reported as an error — `LV_USE_ASSERT_MALLOC` spins and the task watchdog
  reboots, which looks like a display bring-up that restarts forever and sends
  you hunting in the wrong subsystem.
- **The generated `lv_conf.h` is refreshed when the library's default
  changes.** It is written into `.pio/libdeps/`, so "never overwrite" meant a
  library upgrade could silently fail to apply — raising `LV_MEM_SIZE` above
  did nothing until the stale copy was deleted by hand. The hook now records a
  hash of what it wrote: an untouched copy is refreshed, an edited one is kept
  and reported.
- **Widgets no longer escape their panel.** The checkbox, dropdown and roller
  now fit their font to the width their longest text needs, not only to the
  height; the slider's track is inset by the knob's overhang instead of running
  past the tile edge.
- **The arc and spinner tracks no longer threshold to white.** They used the
  theme's `dim`, a light grey that the 1-bit reduction rounds *up*, so track
  and indicator both came out white and the widget read as a solid ring.
- **A pressed button shows its caption again.** The inverted text colour was
  set on the label, and a child does not receive its parent's states, so it
  never applied — white caption on a white face. It is set on the button now
  and inherited.
- **An unlit LED is visible.** `lv_led` scales every colour it draws by its
  brightness, the outline included, so the ring meant to show an unlit lamp
  faded out with the fill. `wua_led()` no longer uses that widget.

### Changed

- **The default `lv_conf.h` now enables LVGL's whole widget set**, not only the
  six widgets this library's own primitives are built from. Found while writing
  the demo catalogue: `lv_slider_create()` failed to compile in a sketch that
  had done nothing wrong, and the file to fix is generated into
  `.pio/libdeps/` and regenerated from under the user — which makes a
  zero-configuration install a trap rather than a convenience. Unused widgets
  are dropped at link time, so the cost is compile time, not flash. `lottie`,
  `gif` and `3dtexture` stay off: they need external decoders.

- **`setResolution()` is now the one function that decides the resolution, and
  `begin()` takes no argument.** Called in `setup()` before `begin()` it simply
  selects the mode — nothing is displaying, so there is nothing to tear down
  and no restart. Called after `begin()` it is a change rather than a choice,
  and restarts into the new mode as before.

  This replaces `begin(res)`, which had two problems. A mode saved by
  `setResolution()` used to outrank its argument *permanently*, so once any
  sketch had switched mode every sketch flashed afterwards silently came up in
  that mode — found on hardware, where four demos each asking for a different
  resolution all displayed the same one. And with the mode settable from two
  places, which one won was never obvious. Now the request `setResolution()`
  makes from a running sketch is a one-shot consumed by the restart it
  performs, and it is the only thing that can outrank `setup()`.

### Removed

- `storedResolution()` and `clearStoredResolution()`. With a one-shot request
  there is no stored mode to read or forget, and persistence across power
  cycles is a policy the application should own — the README shows the ten
  lines that implement it.
- The `res` parameter of `begin()`. `setResolution()` selects the mode.

### Planned

- Changing mode **without** a restart, as a separate call. It cannot be
  `setResolution()`: the widget primitives resolve their pixel sizes when they
  are created, so an in-place switch requires the application to rebuild its
  own widgets — which is precisely what the restart does for it today.

### Verified

- **0.3.0 validated on hardware**: all five display modes bring up and render,
  the runtime mode switch works end to end (store, restart, re-negotiate,
  repaint), the widget primitives keep their proportions across every mode, and
  the telemetry readout tracks the engine temperature.

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
