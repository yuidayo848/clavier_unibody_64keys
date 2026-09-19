/*
 * このボードで確認された、kscanが完全に無反応になる2種類の不具合に対処する
 * 「見張り役」(ZMK本体コード側の不具合のため、オーバーレイ/Kconfigだけでは直せない)。
 *
 * 不具合A: kscan_gpio_matrix.c の kscan_matrix_read() は、スキャン中に1回でも
 *          I2Cエラーが発生するとワークアイテムの再スケジュールをせずに抜けてしまい、
 *          スキャン処理そのものが無音のまま永久に停止する(PM状態はACTIVEのまま)。
 *          → kscan_enable_callback() を呼び直せば、そのままread()が再実行されて回復する。
 *
 * 不具合B: 起動時、physical_layouts.c が唯一 PM_DEVICE_ACTION_RESUME を呼んで
 *          kscanを有効化するが、その最初の1回のスキャンでI2Cエラーが起きると
 *          pm_device_action_run() の戻り値がチェックされずに握りつぶされ、PM状態が
 *          SUSPENDED(ピン未設定)のまま永久に固定されてしまう。この状態では
 *          kscan_enable_callback() を呼んでもピンが再設定されないため回復しない。
 *          → pm_device_action_run(RESUME) を呼び直す必要がある(setup_pins()が
 *            再実行され、ピンが正しく再設定される)。既にACTIVEなら-EALREADYが
 *            返るだけで実害はない。
 *
 * どちらの状態からでも確実に復帰できるよう、数秒おきに両方を呼び出す。
 * どちらも、既に押している途中のキーのデバウンス状態(data->matrix_state)には
 * 一切触れないため、正常動作中に呼んでも実害はない。
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kscan_watchdog, LOG_LEVEL_INF);

#define WATCHDOG_INTERVAL_MS 3000

static void watchdog_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(watchdog_work, watchdog_work_handler);

static void watchdog_work_handler(struct k_work *work) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    int resume_err = pm_device_action_run(kscan_dev, PM_DEVICE_ACTION_RESUME);
    if (resume_err && resume_err != -EALREADY) {
        LOG_ERR("kscan_watchdog: pm resume failed: %d", resume_err);
    }

    int enable_err = kscan_enable_callback(kscan_dev);
    if (enable_err) {
        LOG_ERR("kscan_watchdog: kscan_enable_callback failed: %d", enable_err);
    }

    k_work_reschedule(&watchdog_work, K_MSEC(WATCHDOG_INTERVAL_MS));
}

static int kscan_watchdog_init(void) {
    k_work_schedule(&watchdog_work, K_MSEC(WATCHDOG_INTERVAL_MS));
    return 0;
}

SYS_INIT(kscan_watchdog_init, APPLICATION, 97);
