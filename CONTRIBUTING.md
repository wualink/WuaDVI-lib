# Contributing to WuaDVI-lib

**We're glad you're here.** WuaDVI is built by Wualink (member of Wualabs), and
bug reports, ideas and code from the community are all welcome.

This repository is the **driver library** for the WuaDVI board. Related repos:

| Repo | What it is |
|---|---|
| [WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite) | RP2354B display-engine firmware |
| **WuaDVI-lib** (this) | Library: board management, transport, LVGL primitives |
| WuaDVI-examples | Demo sketches built on this library |
| WuaDVI-net | Networked application: remote control over IP |

If your issue is about the DVI signal itself, the monochrome modes or the RP's
resolutions, it probably belongs in **WuaDVI-rp-lite**. If unsure, open it here
and we will move it.

- [Ways to contribute](#ways-to-contribute)
- [Submitting code](#submitting-code)
- [Formatting](#formatting)
- [Coding standards](#coding-standards)
- [Things that are load-bearing](#things-that-are-load-bearing)
- [Versioning](#versioning)
- [Licensing of contributions](#licensing-of-contributions)
- [Code of conduct](#code-of-conduct)

---

## Ways to contribute

| I want to… | Do this |
|---|---|
| Report a bug | Open a [Bug report issue](https://github.com/wualink/WuaDVI-lib/issues/new/choose) |
| Suggest a feature | Open a [Feature request issue](https://github.com/wualink/WuaDVI-lib/issues/new/choose) |
| Send code | Open a Pull Request and fill in the template |

Please **open an issue before a large PR** — especially for anything touching
the public API, the wire protocols or the RP firmware pinning. A short
conversation up front saves rework.

**Security issues:** do not open a public issue. Email security@wualabs.com.

---

## Submitting code

1. **Fork** and branch off `main` (`feat/…`, `fix/…`, `docs/…`).
2. Keep the change focused — one logical change per PR.
3. **Format**: `clang-format -i src/*.h src/*.cpp` (CI rejects unformatted code).
4. **Build**: the examples must compile. CI does this, but check locally first.
5. **Test on hardware** for anything touching the board, the transport or the
   widgets, and say so in the PR — which resolution(s), and the serial log. CI
   can only prove it *compiles*; it cannot prove the picture is right.
6. **Update the docs and the changelog** — add a line under `[Unreleased]` in
   [`CHANGELOG.md`](CHANGELOG.md).
7. Open the PR against `main`, fill in the template, link the issue
   (`Closes #123`).

CI must be green: **clang-format**, **build** and **Arduino lint**.

**Do not bump the version** in a feature PR unless a maintainer asks — see
[Versioning](#versioning).

---

## Formatting

```bash
pip install clang-format               # same tool CI uses
clang-format -i src/*.h src/*.cpp      # format in place
clang-format --dry-run --Werror src/*.h src/*.cpp   # check, like CI
```

Hand-aligned blocks are fenced with `// clang-format off` / `on` — leave those
fences in place if you edit near them.

---

## Coding standards

Match the surrounding code.

- **English** for all comments and documentation.
- **Doxygen headers** on every non-trivial function: `@brief`, `@param`,
  `@return`. Explain *why*, not just *what* — the reason behind a line is worth
  a sentence, and this codebase has several lines whose reason is not guessable.
- **Naming:** `snake_case` for C-style functions and file-local statics,
  `CamelCase` for types and classes, `UPPER_SNAKE` for macros.
- **Public headers stay light.** Never include a generated payload or a large
  array from a public header — it would be compiled into every translation unit
  that includes the library.
- **The library must not print to the console on the hot path.** A blocking
  write can disturb the rect stream.

---

## Things that are load-bearing

Some constraints here are not style — they are the difference between a working
picture and a blank screen. Before changing the display bring-up, the transport
or the widget internals:

1. **The wire protocols are shared with the RP firmware.** `link_protocol.h` and
   the rect format must stay byte-identical with
   [WuaDVI-rp-lite](https://github.com/wualink/WuaDVI-rp-lite). A change here
   needs a matching change there, landed together, or the two stop talking.
2. **The RP firmware is pinned by version *and* hash.** A git tag can be moved;
   the hash is what actually identifies an artifact. Do not relax that check.
3. **The tile dither is not decoration.** A perfectly flat colour field makes the
   TMDS encoder emit visible periodic banding. The dither breaks it up.
4. **Value labels reserve their widest width on purpose.** Without it the layout
   reflows on every update and widgets visibly dance.
5. **Setters filter redundant updates.** Same value → no redraw → no SPI
   traffic. Removing that puts avoidable load on a link with little margin.
6. **Do not lower LVGL's refresh rate to steady a marginal mode.** It makes it
   worse: fewer refreshes mean larger bursts of rectangles, and bursts are what
   starve the display engine. Measured, not theorised.

---

## Versioning

This library follows [Semantic Versioning](https://semver.org). Because two
projects depend on it, the version is a contract, not a label.

The version appears in **three** places and CI checks they agree with the tag:

- `library.json` (PlatformIO)
- `library.properties` (Arduino IDE)
- the git tag (`vX.Y.Z`)

Releases are cut by maintainers: bump all three, update the changelog, tag.

---

## Licensing of contributions

This project is MIT licensed (© Wualabs LTD). By submitting a contribution you
agree it is licensed under the same terms, and you confirm you have the right to
submit it.

---

## Code of conduct

Be respectful and constructive. Harassment or hostile behaviour is not tolerated
in issues, pull requests or reviews.
