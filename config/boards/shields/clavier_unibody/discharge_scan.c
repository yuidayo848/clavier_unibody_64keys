/*
 * デバッグ用・実験的(診断が終わったら削除する):
 *
 * MCP23017側のRow線(5本)にはプルダウン抵抗が無く、フローティングになっている
 * 疑いが強い。ハードウェア改造ができない前提で、標準のzmk,kscan-gpio-matrix
 * ドライバを使わず、自前のスキャン処理を実装する:
 *
 *   各列を駆動する直前に、5本のRow線を一瞬だけ「出力・LOW」にして強制放電し、
 *   すぐに「入力」へ戻してから読む。これにより、フローティングによる不定な
 *   読み取りを避け、押されていない状態を確実にLOWとして検出できることを狙う。
 *
 * 標準のkscan0デバイス(devicetree上はそのまま残してある)とは独立して動作する。
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <zmk/matrix_transform.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>

LOG_MODULE_REGISTER(discharge_scan, LOG_LEVEL_INF);

#define KSCAN_NODE DT_CHOSEN(zmk_kscan)
#define TRANSFORM_NODE DT_NODELABEL(default_transform)

#define NUM_ROWS 5
#define NUM_COLS 16

ZMK_MATRIX_TRANSFORM_EXTERN(TRANSFORM_NODE);
#define TRANSFORM ZMK_MATRIX_TRANSFORM_T_FOR_NODE(TRANSFORM_NODE)

static const struct gpio_dt_spec rows[NUM_ROWS] = {
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, row_gpios, 0), GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, row_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, row_gpios, 2), GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, row_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, row_gpios, 4),
};

static const struct gpio_dt_spec cols[NUM_COLS] = {
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 0),  GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 1),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 2),  GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 3),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 4),  GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 5),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 6),  GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 7),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 8),  GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 9),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 10), GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 11),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 12), GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 13),
    GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 14), GPIO_DT_SPEC_GET_BY_IDX(KSCAN_NODE, col_gpios, 15),
};

static bool key_state[NUM_ROWS][NUM_COLS];
static bool ready;
static uint32_t scan_count;
static uint32_t event_count;

static void discharge_rows(void) {
    for (int r = 0; r < NUM_ROWS; r++) {
        gpio_pin_configure_dt(&rows[r], GPIO_OUTPUT_INACTIVE);
    }
    k_busy_wait(100);
    for (int r = 0; r < NUM_ROWS; r++) {
        gpio_pin_configure_dt(&rows[r], GPIO_INPUT);
    }
}

static void scan_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(scan_work, scan_work_handler);

static void scan_work_handler(struct k_work *work) {
    if (!ready) {
        goto reschedule;
    }

    scan_count++;

    for (int c = 0; c < NUM_COLS; c++) {
        discharge_rows();

        gpio_pin_set_dt(&cols[c], 1);
        k_busy_wait(100);

        for (int r = 0; r < NUM_ROWS; r++) {
            int val = gpio_pin_get_dt(&rows[r]);
            bool pressed = (val == 1);
            if (pressed != key_state[r][c]) {
                key_state[r][c] = pressed;
                int32_t position = zmk_matrix_transform_row_column_to_position(TRANSFORM, r, c);
                if (position >= 0) {
                    event_count++;
                    LOG_INF("discharge_scan: r=%d c=%d pos=%d pressed=%d (event #%u)", r, c,
                            position, pressed, event_count);
                    raise_zmk_position_state_changed((struct zmk_position_state_changed){
                        .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                        .position = (uint32_t)position,
                        .state = pressed,
                        .timestamp = k_uptime_get(),
                    });
                }
            }
        }

        gpio_pin_set_dt(&cols[c], 0);
    }

    if (scan_count % 100 == 0) {
        LOG_INF("discharge_scan: alive, scan_count=%u event_count=%u", scan_count, event_count);
    }

reschedule:
    k_work_reschedule(&scan_work, K_MSEC(20));
}

static int discharge_scan_init(void) {
    LOG_INF("discharge_scan: init starting");

    for (int c = 0; c < NUM_COLS; c++) {
        if (!device_is_ready(cols[c].port)) {
            LOG_ERR("discharge_scan: col %d port not ready", c);
            return 0;
        }
        gpio_pin_configure_dt(&cols[c], GPIO_OUTPUT_INACTIVE);
    }
    for (int r = 0; r < NUM_ROWS; r++) {
        if (!device_is_ready(rows[r].port)) {
            LOG_ERR("discharge_scan: row %d port not ready", r);
            return 0;
        }
        gpio_pin_configure_dt(&rows[r], GPIO_INPUT);
    }

    ready = true;
    LOG_INF("discharge_scan: init complete, scan loop starting");
    k_work_schedule(&scan_work, K_MSEC(1000));
    return 0;
}

SYS_INIT(discharge_scan_init, APPLICATION, 95);
