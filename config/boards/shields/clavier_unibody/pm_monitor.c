/*
 * デバッグ用(診断が終わったら削除する):
 * 「書き込み直後は動くが、常に約8秒後に全キー無反応になる」という現象の原因を
 * 特定するため、キースキャンデバイスのPM(電源管理)状態を1秒おきにログ出力する。
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pm_monitor, LOG_LEVEL_INF);

static void monitor_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(monitor_work, monitor_work_handler);

static void monitor_work_handler(struct k_work *work) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    enum pm_device_state state = PM_DEVICE_STATE_OFF;
    int ret = pm_device_state_get(kscan_dev, &state);

    LOG_INF("pm_monitor: uptime=%lld ms, kscan pm_state_get ret=%d state=%d (0=active,1=suspended,2=off,3=susp_to_susp)",
            k_uptime_get(), ret, state);

    k_work_reschedule(&monitor_work, K_MSEC(500));
}

static int pm_monitor_init(void) {
    k_work_schedule(&monitor_work, K_MSEC(500));
    return 0;
}

SYS_INIT(pm_monitor_init, APPLICATION, 96);
