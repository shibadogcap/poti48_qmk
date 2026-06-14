/* ps2_split.c
 * PS/2 -> split-pointing bridge for right-side-only TrackPoint.
 * Runs on the slave side (right) to capture PS/2 reports and forward
 * them via the split pointing device transport to the master (left).
 */
#include QMK_KEYBOARD_H
#include <hardware/gpio.h>
#include <stdint.h>
#include <stdbool.h>

// =========================================================
// Left-side PS/2 init skip
// PS/2 device is connected only on the right side.
// =========================================================
static bool is_left_side = false;

void keyboard_pre_init_user(void) {
    // Read GP29 directly (QMK split isn't ready yet at pre_init)
    gpio_init(29);
    gpio_set_dir(29, GPIO_IN);
    gpio_pull_up(29);
    wait_us(100);
    is_left_side = gpio_get(29);  // HIGH=left, LOW=right

    // On left side, force PS/2 pins LOW so init fails fast
    if (is_left_side) {
        gpio_init(8);
        gpio_init(9);
        gpio_set_dir(8, GPIO_OUT);
        gpio_set_dir(9, GPIO_OUT);
        gpio_put(8, 0);
        gpio_put(9, 0);
    }
}

void ps2_mouse_init_user(void) {
    // Nothing extra needed
}

// Pending report storage
static volatile bool      ps2_report_pending = false;
static report_mouse_t     ps2_pending_report;

// Debug logging helper
#ifdef CONSOLE_ENABLE
#define LOG(fmt, ...) uprintf("%08lu: " fmt "\n", timer_read32(), ##__VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

// PS/2 mouse moved hook: store report and zero it out to prevent
// the PS/2 driver from sending HID directly (we use split pointing).
void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
    // Debug LED toggle (GP17) to indicate ISR activity
    static bool led_inited = false;
    if (!led_inited) {
        gpio_init(17);
        gpio_set_dir(17, GPIO_OUT);
        led_inited = true;
    }
    gpio_put(17, !gpio_get(17));

    // Store the report for the pointing device transport
    ps2_pending_report = *mouse_report;
    ps2_report_pending = true;

    // Zero out to suppress direct HID send by PS/2 driver
    mouse_report->x = 0;
    mouse_report->y = 0;
    mouse_report->v = 0;
    mouse_report->h = 0;
    mouse_report->buttons = 0;
}

// Called by ps2_pointing.c (custom pointing driver) to retrieve the
// latest pending PS/2 report for the split transport.
report_mouse_t ps2_pointing_get_report(report_mouse_t mouse_report) {
    if (ps2_report_pending) {
        ps2_report_pending = false;
        report_mouse_t rpt = ps2_pending_report;
        ps2_pending_report.x = 0;
        ps2_pending_report.y = 0;
        ps2_pending_report.v = 0;
        ps2_pending_report.h = 0;
        ps2_pending_report.buttons = 0;
        return rpt;
    }
    return mouse_report;
}
