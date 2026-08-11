# WuaDVI-lib

Driver library for the **WuaDVI board** — HDMI/DVI output from an ESP32, with
LVGL widgets that adapt themselves to every supported resolution.

Wualink, member of Wualabs — [wualabs.com](https://wualabs.com)

> **Status: 0.3.0 — validated on hardware.** The board API, transport, widget
> primitives and runtime resolution selection all work on the WuaDVI board, in
> every one of the five display modes. See [Roadmap](#roadmap).

---

## What this is

The WuaDVI board is two microcontrollers with one job:

| | Role |
|---|---|
| **ESP32-C3** | Application processor — your sketch, and LVGL |
| **RP2354B** | Display engine — turns a framebuffer into a DVI/HDMI signal |

They talk over SPI. This library hides that entirely: you write an ordinary
Arduino sketch, draw with LVGL, and pixels come out of the HDMI connector.

It takes care of:

- **Bringing up the RP2354B**, including flashing its firmware when it differs
  from the one this library ships. Your sketch never has to know the second chip
  exists.
- **Streaming the UI** — LVGL's dirty rectangles are packed and sent over a
  25 MHz SPI link, with the wire protocol, pacing and error recovery handled.
- **Resolution independence** — the widget primitives take percentages, not
  pixels, and resolve fonts, padding and geometry for whichever mode is running.
- **Board telemetry** — the RP's on-die temperature comes back on the otherwise
  idle MISO line and doubles as a health heartbeat, so a display engine that
  resets is detected and recovered automatically.

---

## Supported resolutions

| Mode | Framebuffer | Colour | DVI output |
|---|---|---|---|
| `WUADVI_RES_320x240` | 150 KB | RGB565 | 640 × 480p60 (pixel-doubled) |
| `WUADVI_RES_400x240` | 192 KB | RGB565 | 800 × 480p60 (pixel-doubled) |
| `WUADVI_RES_640x480x1` | 37.5 KB | 1-bit monochrome | 640 × 480p60 |
| `WUADVI_RES_800x600x1` | 58.6 KB | 1-bit monochrome | 800 × 600p60 |
| `WUADVI_RES_1280x720x1` | 112.5 KB | 1-bit monochrome | 1280 × 720p**30** |

Full colour is limited by the RP2350's 520 KB of SRAM — 640 × 480 in RGB565
would need ~600 KB. The monochrome modes exist because the 1-bit TMDS encoder is
cheap enough to reach native resolutions that RGB565 cannot.

`1280x720x1` runs at 30 Hz (60 Hz would need a ~742 MHz TMDS bit clock) and
`800x600x1` is the least stable mode of the set. Both are documented in the
[display-engine firmware](https://github.com/wualink/WuaDVI-rp-lite).

---

## Quick start

```cpp
#include <WuaDVI.h>

WuaDVI dvi;

void setup() {
    Serial.begin(115200);

    // Brings up the RP2354B (flashing it if needed), negotiates the mode,
    // starts LVGL and the rect stream.
    if (!dvi.begin(WUA_RES_640x480x1)) {
        Serial.println(dvi.lastError());
        return;
    }

    lv_obj_t *tile = wua_tile(lv_screen_active(), "Hello", 90, 60,
                              wua_theme()->tile);
    wua_label(tile, "WuaDVI", 12, wua_theme()->accent);
}

void loop() {
    dvi.loop();   // pumps LVGL, streams dirty rects, watches the display engine
}
```

---

## Installation

### PlatformIO (recommended)

One line. LVGL is installed for you, and so is the pinned display-engine
firmware:

```ini
lib_deps = https://github.com/wualink/WuaDVI-lib.git#v0.3.0
```

Pin an exact version rather than a floating range — this library pins the
display-engine firmware in turn, and a reproducible build wants the whole chain
fixed.

Nothing else is required: no `lv_conf.h`, no build flags, no extra dependency.
If you want your own LVGL configuration, see
[LVGL configuration](#lvgl-configuration) below.

### Arduino IDE

Download a release ZIP and use **Sketch → Include Library → Add .ZIP Library**,
or clone into your `libraries/` folder. Install **LVGL 9.x** from the Library
Manager as well.

> **⚠️ Arduino IDE support is not complete yet.** The Arduino build system
> cannot run library build scripts, and this library relies on one to fetch and
> embed the RP2354B firmware — so an Arduino IDE build currently fails on the
> missing payload. **Use PlatformIO for now.** Shipping a pre-generated payload
> in the repository, which would make the Arduino IDE work identically, is
> tracked for a coming release.

---

## Requirements

| | |
|---|---|
| Board | WuaDVI (ESP32-C3 + RP2354B) |
| Framework | Arduino, ESP32 core 3.x |
| LVGL | 9.x |

### LVGL configuration

**You do not need to do anything here to get started.** LVGL is normally
configured per *application* through `lv_conf.h`, which is why adding an
LVGL-based library usually fails on a missing configuration file. This library
avoids that: if your project does not supply an `lv_conf.h`, it installs a
validated default (`config/lv_conf_default.h`) where LVGL looks for one.

To use your own configuration instead, provide an `lv_conf.h` in your project —
the library never overwrites a file that is already there — or set `LV_CONF_PATH`
/ `LV_CONF_INCLUDE_SIMPLE`, which bypasses the mechanism entirely. Each build
prints which configuration is in effect:

```
[WuaDVI-lib] lv_conf.h  : provided by the project
[WuaDVI-lib] lv_conf.h  : supplied by the library -> .pio/libdeps/<env>/lv_conf.h
```

[`extras/lv_conf_reference.h`](extras/) is a commented starting point for your
own. Whatever you use must satisfy at least:

| Setting | Required value | Why |
|---|---|---|
| `LV_COLOR_DEPTH` | `16` | The wire format is RGB565 |
| `LV_DRAW_SW_SUPPORT_ARGB8888` | `1` | `lv_bar` composites its indicator through an ARGB8888 layer; with this off the bar silently refuses to fill |
| `LV_FONT_MONTSERRAT_14/18/24/32/48` | `1` | The primitives step between these when scaling text to the active resolution |

The library validates these at compile time and fails with an explanatory
`#error` rather than misbehaving at runtime.

---

## Architecture

Three layers, so simple sketches stay simple and advanced ones stay possible:

| Layer | Surface | For |
|---|---|---|
| **L0 — Board** | `begin()`, `loop()`, `resolution()`, `temperature()`, `ready()`, `stats()` | Most sketches |
| **L1 — Transport** | Direct access to the rect stream and control link | Diagnostics, custom pipelines |
| **L2 — Widgets** | `wua_tile`, `wua_gauge`, `wua_meter`, `wua_clock`, `wua_label` | Building interfaces |

L0 is meant to be enough on its own. If a sketch has to reach into L1 to do
something ordinary, that is a gap in L0 — please open an issue.

### Choosing the display mode

The mode is a runtime value, not a build flag:

```cpp
dvi.begin(WUA_RES_800x600x1);      // start in a mode

dvi.setResolution(WUA_RES_1280x720x1);   // change it — stores and restarts
```

`setResolution()` **does not return**: it saves the mode and restarts the
board. That is not a shortcut — the display engine reboots into a new mode
anyway (its clocks and framebuffer are fixed once scanout starts), and this
side has to resize its render buffer and rebuild the interface regardless.
Restarting both together is the one sequence that is always consistent.

The stored mode wins over the argument to `begin()`, so a sketch written as
`dvi.begin()` follows whatever was last chosen. `WuaDVI::storedResolution()`
reads it and `WuaDVI::clearStoredResolution()` forgets it.

> **Worth knowing before it surprises you.** That precedence applies to *every*
> sketch, so once a mode has been stored, the next sketch you flash comes up in
> it and its own `begin()` argument is ignored. `begin()` prints a line saying
> so. Clear it with `clearStoredResolution()`, or press `c` on the console
> below.

#### A console for free

Rather than wiring your own mode switcher, forward serial keys to the library:

```cpp
void loop() {
    dvi.loop();
    while (Serial.available() > 0)
        dvi.consoleKey((char)Serial.read());
}
```

`1`–`5` select a mode, `c` forgets the stored one, `i` prints status, `?` lists
the keys — and `WuaDVI::printConsoleHelp()` shows the list at startup.
`consoleKey()` returns `false` for anything it does not recognise, so a sketch
that reads its own commands can share the same stream.

### Theming

The primitives carry the *mechanism* (resolution-independent sizing, the
flat-field dither that suppresses TMDS banding, redraw suppression that keeps
the SPI link quiet); your application supplies the *identity*:

```cpp
static const wua_theme_t my_theme = {
    .bg     = lv_color_hex(0x000000),
    .tile   = lv_color_hex(0x101820),
    .text   = lv_color_hex(0xFFFFFF),
    .accent = lv_color_hex(0x00A0FF),
    /* ... */
};

wua_theme_set(&my_theme);   // false if the theme is not monochrome-safe
```

`wua_theme_set()` rejects a palette whose foregrounds would not survive the
1-bit luminance threshold — in the monochrome modes an unchecked colour choice
becomes an invisible one, and it is better to hear about it at startup than to
discover it on a different display mode.

---

## The display-engine firmware

The RP2354B runs [WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite),
a single universal image that carries every resolution and selects one at
runtime. This library **pins an exact release** of it:

```
assets/rp-firmware/VERSION    the release, e.g. 1.0.0
assets/rp-firmware/SHA256     that asset's expected hash
```

Under PlatformIO the pinned image is downloaded on the first build, verified
against the hash and cached; the binary itself is not vendored in git. Pinning
by tag alone is not enough — a git tag can be moved — so a hash mismatch fails
the build rather than flashing an unknown image.

At startup the library compares the version the RP reports against the one it
ships and reflashes it over the RP2350 ROM UART boot when they differ. In normal
use this happens once and is invisible.

---

## Roadmap

| Version | Contents |
|---|---|
| 0.1.0 | Packaging, RP firmware pinning and build hook |
| 0.2.0 | Board API (L0), transport (L1), widget primitives (L2) and theming |
| **0.3.0** | Runtime resolution selection — **current** |
| 1.0.0 | Stable API |

Related repositories:

- **[WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite)** — RP2354B
  display-engine firmware
- **WuaDVI-examples** — demo sketches built on this library
- **WuaDVI-net** — networked application: remote control and events over IP

---

## Contributing

Contributions are welcome — issues, ideas and pull requests. Please read
`CONTRIBUTING.md` before opening a PR.

---

## License

Released under the [MIT License](LICENSE) — © 2026 Wualabs LTD.
