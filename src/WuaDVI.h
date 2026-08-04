/**
 * @file WuaDVI.h
 * @brief Public entry point of WuaDVI-lib — PHASE-0 SKELETON.
 *
 * Only enough surface to prove the packaging mechanism. The real API (the
 * WuaDVI board class, the transport and the widget primitives) lands in
 * phase 1; see refactor-plan.md.
 *
 * NOTE: this header deliberately does NOT include the generated RP payload
 * header. That payload is a large C array and is library-internal — see
 * scripts/library_build.py.
 */
#pragma once

#include <stdint.h>

/** @return Version string of the RP2354B firmware this library ships. */
const char *wuadvi_rp_firmware_version(void);
