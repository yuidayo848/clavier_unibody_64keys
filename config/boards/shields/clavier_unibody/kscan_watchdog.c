/*
 * kscanドライバ(kscan_gpio_matrix.c)には、スキャン中に1回でもI2Cエラーが発生すると
 * ワークアイテムの再スケジュールを行わずにそのまま処理を抜けてしまう不具合があり、
 * これによりスキャン処理が無音のまま永久に停止する(以後キーが一切反応しなくなる)。
 *
 * この不具合はドライバ内部(このリポジトリ外のZMK本体コード)にあり、オーバーレイや
 * Kconfigだけでは修正できないため、数秒おきに kscan_enable_callback() を強制的に
 * 呼び出すことでスキャン処理を再開させる「見張り役」を用意する。
 *
 * kscan_matrix_enable() は scan_time を現在時刻にリセットして即座に1回スキャンを
 * 実行するだけで、既に押している途中のキーのデバウンス状態(data->matrix_state)には
 * 一切触れない。そのため、正常にスキャンが動いている最中にこれを呼んでも
 * (ワークアイテムの再スケジュールがずれるだけで)実害はなく、逆にスキャンが
 * 停止してしまっている場合は確実に復帰できる。
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kscan_watchdog, LOG_LEVEL_INF);

#define WATCHDOG_INTERVAL_MS 3000

static void watchdog_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(watchdog_work, watchdog_work_handler);

static void watchdog_work_handler(struct k_work *work) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    int err = kscan_enable_callback(kscan_dev);
    if (err) {
        LOG_ERR("kscan_watchdog: kscan_enable_callback failed: %d", err);
    }

    k_work_reschedule(&watchdog_work, K_MSEC(WATCHDOG_INTERVAL_MS));
}

static int kscan_watchdog_init(void) {
    k_work_schedule(&watchdog_work, K_MSEC(WATCHDOG_INTERVAL_MS));
    return 0;
}

SYS_INIT(kscan_watchdog_init, APPLICATION, 97);
