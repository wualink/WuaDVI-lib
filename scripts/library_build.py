"""library_build.py — WuaDVI-lib build hook (declared by library.json).

Runs before the library compiles and does two things a consuming application
should never have to do itself:

  1. Makes the pinned RP2354B firmware available — downloading the release
     asset if it is not cached, verifying it against the pinned SHA-256,
     converting the UF2 to the flat flash image the RAM stub programs, and
     emitting it (with the stub) as a C array the library links in.
  2. Supplies a working LVGL configuration when the project has none, so
     `lib_deps = WuaDVI` is genuinely all that is required.

── Two PlatformIO details this depends on ─────────────────────────────────────
1. A library extraScript runs via SCons `SConscript(..., exports={...})`, which
   `exec`s the file WITHOUT defining `__file__`.  PlatformIO exports
   `pio_lib_builder` for exactly this: `pio_lib_builder.path` is the library
   root, wherever the library was installed from.
2. The generated header goes on the LIBRARY's include path and is included only
   by the library's own sources.  The payload is a ~100 KB C array; putting it
   behind a public header would compile it into every translation unit that
   includes WuaDVI.h.

── Where the firmware comes from ──────────────────────────────────────────────
The binary is not vendored in git.  Two small text files pin it:

    assets/rp-firmware/VERSION   the release to use, e.g. "1.0.0"
    assets/rp-firmware/SHA256    that asset's expected SHA-256

Pinning by tag alone is not enough — a git tag can be moved after the fact — so
a hash mismatch fails the build rather than flashing an unknown image.
"""
import hashlib
import json
import os
import re
import struct
import urllib.error
import urllib.request
import zlib

Import("env", "pio_lib_builder")  # noqa: F821  (injected by SCons/PlatformIO)

LIB_DIR = pio_lib_builder.path    # noqa: F821  (see note 1 above)
ASSETS_DIR = os.path.join(LIB_DIR, "assets", "rp-firmware")
STUB_PATH = os.path.join(LIB_DIR, "tools", "rp-flash-stub", "stub.bin")

RP_REPO = "wualink/WuaDVI-rp-lite"
DOWNLOAD_TIMEOUT_S = 120

# UF2 container constants (https://github.com/microsoft/uf2)
UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
UF2_FAMILY_RP2350_ARM_S = 0xE48BFF59

XIP_BASE = 0x10000000
FLASH_MAX_SIZE = 2 * 1024 * 1024  # RP2354B in-package flash

# Every mode the universal RP image must contain.  Checking the splash captions
# catches a stale or wrong asset at build time instead of at first boot.
REQUIRED_MODES = (
    b"320x240 RGB565",
    b"400x240 RGB565",
    b"640x480 mono",
    b"800x600 mono",
    b"1280x720 mono",
)


def _die(msg):
    raise SystemExit(f"[WuaDVI-lib] error: {msg}")


# ── Fetching the pinned release ───────────────────────────────────────────────
def _http_get(url, accept):
    """GET a URL, honoring GITHUB_TOKEN when the environment provides one.

    The token is optional (the RP repository is public) but lifts GitHub's low
    anonymous rate limit, which matters on shared CI runners.
    """
    req = urllib.request.Request(
        url, headers={"Accept": accept, "User-Agent": "WuaDVI-lib-build"})
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req, timeout=DOWNLOAD_TIMEOUT_S) as resp:
        return resp.read()


def _download(dest_path, name, tag):
    """Fetch one release asset into dest_path, atomically."""
    print(f"[WuaDVI-lib] {name} not cached - downloading from {RP_REPO} {tag}")
    api = f"https://api.github.com/repos/{RP_REPO}/releases/tags/{tag}"
    try:
        release = json.loads(_http_get(api, "application/vnd.github+json"))
    except urllib.error.HTTPError as e:
        _die(f"cannot read release {tag} of {RP_REPO} (HTTP {e.code}). "
             f"Check assets/rp-firmware/VERSION, or place {name} in "
             f"{ASSETS_DIR} manually.")
    except Exception as e:  # offline, DNS, TLS, timeout...
        _die(f"cannot reach GitHub to fetch {name} ({type(e).__name__}: {e}). "
             f"Build once with a connection to cache it, or place {name} in "
             f"{ASSETS_DIR} manually.")

    asset = next((a for a in release.get("assets", []) if a["name"] == name), None)
    if asset is None:
        available = ", ".join(a["name"] for a in release.get("assets", [])) or "none"
        _die(f"release {tag} has no asset named {name} (found: {available})")

    try:
        blob = _http_get(asset["url"], "application/octet-stream")
    except Exception as e:
        _die(f"downloading {name} failed ({type(e).__name__}: {e})")

    # Write via a temporary file so an interrupted download can never leave a
    # truncated .uf2 behind that a later build would happily embed.
    tmp = dest_path + ".part"
    with open(tmp, "wb") as f:
        f.write(blob)
    os.replace(tmp, dest_path)
    print(f"[WuaDVI-lib] downloaded {name} ({len(blob)} B)")


def _ensure_uf2(version):
    """Return the path of the pinned UF2, fetching and verifying as needed."""
    name = f"WuaDVI-rp-lite-v{version}.uf2"
    path = os.path.join(ASSETS_DIR, name)

    sha_file = os.path.join(ASSETS_DIR, "SHA256")
    expected = None
    if os.path.isfile(sha_file):
        expected = open(sha_file).read().split()[0].strip().lower()
        if not re.fullmatch(r"[0-9a-f]{64}", expected):
            _die(f"{sha_file} must contain a 64-hex-digit SHA-256")

    if not os.path.isfile(path):
        os.makedirs(ASSETS_DIR, exist_ok=True)
        _download(path, name, f"v{version}")

    if expected is not None:
        got = hashlib.sha256(open(path, "rb").read()).hexdigest()
        if got != expected:
            _die(f"{name} SHA-256 mismatch\n"
                 f"           expected {expected}\n"
                 f"           got      {got}\n"
                 f"         Delete it to re-download, or re-pin VERSION/SHA256.")
    else:
        print(f"[WuaDVI-lib] warning: no SHA256 pin - {name} is not verified")

    return path, name


# ── UF2 → flat image ──────────────────────────────────────────────────────────
def _uf2_to_bin(path):
    """Convert a UF2 file into flat bytes, gaps filled with 0xFF."""
    data = open(path, "rb").read()
    if len(data) % 512 != 0:
        _die(f"{os.path.basename(path)}: size {len(data)} is not a multiple of 512")

    chunks = []
    for off in range(0, len(data), 512):
        blk = data[off:off + 512]
        m0, m1, flags, target, psize = struct.unpack_from("<IIIII", blk, 0)
        family = struct.unpack_from("<I", blk, 28)[0]
        mend = struct.unpack_from("<I", blk, 508)[0]
        if m0 != UF2_MAGIC_START0 or m1 != UF2_MAGIC_START1 or mend != UF2_MAGIC_END:
            _die(f"invalid UF2 block at offset {off}")
        if (flags & UF2_FLAG_FAMILY_ID) and family != UF2_FAMILY_RP2350_ARM_S:
            _die(f"unexpected UF2 family 0x{family:08X} (want RP2350 ARM-S)")
        if psize == 0 or psize > 476:
            _die(f"invalid payload size {psize} at offset {off}")
        chunks.append((target, blk[32:32 + psize]))

    chunks.sort(key=lambda c: c[0])
    base = chunks[0][0]
    end = max(t + len(p) for t, p in chunks)
    if base != XIP_BASE:
        _die(f"image base 0x{base:08X} != XIP base 0x{XIP_BASE:08X}")
    if end - base > FLASH_MAX_SIZE:
        _die(f"image size {end - base} exceeds the 2 MB internal flash")

    img = bytearray(b"\xFF" * (end - base))
    for target, payload in chunks:
        img[target - base:target - base + len(payload)] = payload
    return bytes(img)


def _c_array(name, blob):
    lines = [f"static const uint8_t {name}[{len(blob)}] = {{"]
    for i in range(0, len(blob), 16):
        lines.append("    " + "".join(f"0x{b:02X}," for b in blob[i:i + 16]))
    lines.append("};")
    return "\n".join(lines)


# ── Generation ────────────────────────────────────────────────────────────────
def _generate_payload():
    version_file = os.path.join(ASSETS_DIR, "VERSION")
    if not os.path.isfile(version_file):
        _die(f"missing {version_file} (it pins which RP release to use)")
    version = open(version_file).read().strip()
    m = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", version)
    if not m:
        _die(f"VERSION must contain M.m.p, got '{version}'")
    maj, mnr, pat = (int(g) for g in m.groups())

    uf2_path, uf2_name = _ensure_uf2(version)

    if not os.path.isfile(STUB_PATH):
        _die(f"missing {STUB_PATH}")
    stub = open(STUB_PATH, "rb").read()

    fw = _uf2_to_bin(uf2_path)

    # Integrity guard: confirm this really is the UNIVERSAL image by checking
    # that every mode's splash caption is present.
    for caption in REQUIRED_MODES:
        if caption not in fw:
            _die(f"{uf2_name} is missing the '{caption.decode()}' mode — it is "
                 f"not the universal RP image (stale or wrong asset?)")

    crc = zlib.crc32(fw) & 0xFFFFFFFF

    header = "\n".join([
        "/* wuadvi_rp_payload.h - GENERATED by WuaDVI-lib - DO NOT EDIT.",
        f" * Source UF2 : {uf2_name}  (universal - all resolutions)",
        f" * Version    : {version}",
        f" * Image      : {len(fw)} bytes, CRC32 0x{crc:08X}",
        " * Library-internal: included by the library's sources only. */",
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define RP_PAYLOAD_VERSION_MAJOR {maj}u",
        f"#define RP_PAYLOAD_VERSION_MINOR {mnr}u",
        f"#define RP_PAYLOAD_VERSION_PATCH {pat}u",
        f'#define RP_PAYLOAD_VERSION_STRING "{version}"',
        f"#define RP_PAYLOAD_FW_SIZE       {len(fw)}u",
        f"#define RP_PAYLOAD_FW_CRC32      0x{crc:08X}u",
        f"#define RP_STUB_SIZE             {len(stub)}u",
        "#define WUADVI_RP_PAYLOAD_PRESENT 1",
        "",
        _c_array("RP_STUB_BIN", stub),
        "",
        _c_array("RP_FW_BIN", fw),
        "",
    ])

    out_dir = os.path.join(env.subst("$BUILD_DIR"), "wuadvi_generated")  # noqa: F821
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "wuadvi_rp_payload.h")

    # Only rewrite when the content changes, so incremental builds stay
    # incremental — this header is ~600 KB of text.
    if not (os.path.isfile(out_path) and open(out_path, newline="").read() == header):
        with open(out_path, "w", newline="\n") as f:
            f.write(header)
        print(f"[WuaDVI-lib] embedded {uf2_name}: {len(fw)} B, v{version}, "
              f"CRC32 0x{crc:08X}")
    else:
        print(f"[WuaDVI-lib] payload up to date: v{version}, CRC32 0x{crc:08X}")

    env.Append(CPPPATH=[out_dir])  # noqa: F821  (library env — see note 2)


def _provide_lvgl_config():
    """Drop a working lv_conf.h next to LVGL when the application has none.

    LVGL is configured per APPLICATION: `lv_conf.h` must be visible when LVGL
    itself compiles, which a library cannot normally arrange.  The practical
    consequence is that `lib_deps = WuaDVI` installs LVGL automatically but the
    build then fails on a missing lv_conf.h — a poor first experience.

    LVGL's own fallback lookup is `../../lv_conf.h` relative to its sources,
    i.e. the libdeps root.  So if nothing is there yet, place our validated
    default there and the library works out of the box.

    An application that wants its own configuration simply provides one — its
    file is never overwritten, and setting LV_CONF_PATH or LV_CONF_INCLUDE_SIMPLE
    bypasses this lookup entirely.
    """
    libdeps = env.subst("$PROJECT_LIBDEPS_DIR")  # noqa: F821
    pioenv = env.subst("$PIOENV")                # noqa: F821
    if not libdeps or not pioenv:
        return
    target = os.path.join(libdeps, pioenv, "lv_conf.h")

    if os.path.isfile(target):
        print("[WuaDVI-lib] lv_conf.h  : provided by the project")
        return

    source = os.path.join(LIB_DIR, "config", "lv_conf_default.h")
    if not os.path.isfile(source):
        return  # nothing to offer; LVGL will report the missing config itself

    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(source, "r", encoding="utf-8") as src:
        body = src.read()
    with open(target, "w", encoding="utf-8", newline="\n") as dst:
        dst.write("/* Written by WuaDVI-lib because this project supplied no\n"
                  " * lv_conf.h.  To use your own, put one here or in your\n"
                  " * project and it will not be overwritten.  Source:\n"
                  " * WuaDVI-lib/config/lv_conf_default.h */\n")
        dst.write(body)
    print(f"[WuaDVI-lib] lv_conf.h  : supplied by the library -> {target}")


_generate_payload()
_provide_lvgl_config()
