/* ps2_pointing.c
 * Minimal pointing_device driver wrapper that exposes the PS/2 pending
 * report (from ps2_split.c) to the pointing_device split transaction
 * machinery. This keeps the PS/2 handling on the slave and lets the
 * core split code copy the report to the master for HID send.
 */

#include QMK_KEYBOARD_H

// Forward declaration of the helper implemented in ps2_split.c
report_mouse_t ps2_pointing_get_report(report_mouse_t mouse_report);

// Provide strong implementations for the "custom" pointing driver
// hooks so that when `POINTING_DEVICE_DRIVER = custom` is set the
// core will call these functions to retrieve reports from the PS/2
// stack we've implemented.
void pointing_device_driver_init(void) {
    // PS/2 init is handled elsewhere; nothing needed here.
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    return ps2_pointing_get_report(mouse_report);
}

uint16_t pointing_device_driver_get_cpi(void) {
    return 0;
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    (void)cpi;
}
