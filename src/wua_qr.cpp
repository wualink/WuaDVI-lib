/**
 * @file wua_qr.cpp
 * @brief QR codes — see wua_qr() in wua_ui.h.
 *
 * A thin wrapper over LVGL's `lv_qrcode`, which is off in LVGL's own default
 * and enabled by this library's `lv_conf`. What the wrapper adds is the part
 * that had to be measured rather than assumed: a cap on how large the symbol
 * may be drawn, and a size chosen so LVGL's internal integer scaling lands on
 * exactly the module size asked for.
 *
 * Copyright (c) 2026 Wualabs LTD. MIT licensed.
 */
#include <string.h>

#include "wua_ui.h"
/* wua_screen_w()/h() live here, and wua_ui.h does not pull it in. */
#include "wua_resolution.h"

#if LV_USE_QRCODE

/* The encoder LVGL vendors, used directly to ask how large a payload's symbol
 * will be. The alternative is a capacity table kept in step with it by hand,
 * and a table that drifts would size the canvas wrongly in exactly the case
 * nobody tests: a payload one byte over a version boundary. */
#include <src/libs/qrcode/qrcodegen.h>

/**
 * @brief Modules LVGL adds around the symbol when the quiet zone is on.
 *
 * **Two per side, not the four the QR specification asks for.** LVGL derives
 * its scale by dividing the canvas by `qrcodegen_version2size(version + 1)`,
 * which is the symbol plus four modules in total.
 *
 * This is a deviation, and it is recorded rather than corrected because the
 * measurement says it works: a code drawn this way scanned at 30 cm and at 2 m
 * in all five display modes, including the 1-bit ones. Widening it would mean
 * fighting the widget for no observed gain — but a scanner that struggles is
 * the first place to look, and drawing the code on a wider light background is
 * the fix if one ever does.
 */
#define QR_QUIET_TOTAL 4

lv_obj_t *wua_qr(lv_obj_t *parent, const char *text, int32_t module_px) {
    if (parent == NULL || text == NULL)
        return NULL;

    const size_t len = strlen(text);
    if (len == 0)
        return NULL;

    /* Byte mode, because that is what lv_qrcode_update() encodes with. An
     * uppercase payload would be denser in QR alphanumeric mode, and this
     * widget never uses it — so capitals buy nothing here, whatever a QR
     * capacity table might suggest. */
    const int version = qrcodegen_getMinFitVersion(qrcodegen_Ecc_MEDIUM, len);
    if (version <= 0)
        return NULL; /* payload too long for any symbol */

    const int32_t modules = qrcodegen_version2size(version);
    if (modules <= 0)
        return NULL;

    /* The divisor LVGL itself uses, so the canvas is an exact multiple of it
     * and its integer division yields precisely `module_px`. Sizing to
     * `modules` instead would leave a remainder and quietly draw smaller. */
    const int32_t grid = modules + QR_QUIET_TOTAL;

    if (module_px < 1)
        module_px = 1;
    if (module_px > WUA_QR_MAX_MODULE_PX)
        module_px = WUA_QR_MAX_MODULE_PX;

    /* Fit the parent, not the screen: this may land in a tile. */
    lv_obj_update_layout(lv_screen_active());
    int32_t avail_w = lv_obj_get_content_width(parent);
    int32_t avail_h = lv_obj_get_content_height(parent);
    int32_t avail = (avail_w < avail_h) ? avail_w : avail_h;
    if (avail <= 0) {
        /* An unresolved parent — created before its siblings, as the two-pass
         * screen builds do. Fall back to the screen's shorter axis. */
        avail = (wua_screen_w() < wua_screen_h()) ? (int32_t)wua_screen_w()
                                                  : (int32_t)wua_screen_h();
    }

    while (module_px > 1 && grid * module_px > avail)
        --module_px;

    /* Refuse rather than draw something unscannable. A QR code that cannot be
     * read is worse than an error message, because it looks like it worked. */
    if (grid * module_px > avail)
        return NULL;

    lv_obj_t *qr = lv_qrcode_create(parent);
    if (qr == NULL)
        return NULL;

    lv_qrcode_set_size(qr, grid * module_px);
    /* Black on white regardless of the theme. Inverting a QR code defeats most
     * scanners, and the monochrome modes threshold by luminance — a themed
     * code would be a code that works in some resolutions and not others. */
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_quiet_zone(qr, true);

    if (lv_qrcode_update(qr, text, (uint32_t)len) != LV_RESULT_OK) {
        lv_obj_delete(qr);
        return NULL;
    }
    return qr;
}

#else /* LV_USE_QRCODE */

lv_obj_t *wua_qr(lv_obj_t *parent, const char *text, int32_t module_px) {
    (void)parent;
    (void)text;
    (void)module_px;
    /* Returning NULL rather than failing to link: an application that does not
     * enable the widget still compiles, and finds out at the call site. */
    return NULL;
}

#endif /* LV_USE_QRCODE */
