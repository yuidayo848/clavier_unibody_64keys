/*
 * 標準のzmk,kscan-gpio-matrixドライバは、CONFIG_PM_DEVICEが有効な場合、
 * 「PM_DEVICE_ACTION_RESUME」が発行されて初めて実際のピン設定
 * (行/列のGPIO方向設定など)を行う実装になっている。この基板の構成では、
 * 通常の起動シーケンスの中でこのレジュームが自動的に呼ばれず、
 * キースキャンが一切ピンを設定しないまま(＝全キー無反応のまま)になる
 * ことを確認したため、起動時に明示的にRESUMEを発行してこれを回避する。
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kscan_resume_fix, LOG_LEVEL_INF);

static int force_kscan_resume(void) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    if (!device_is_ready(kscan_dev)) {
        LOG_ERR("kscan_resume_fix: kscan device not ready");
        return 0;
    }

    int ret = pm_device_action_run(kscan_dev, PM_DEVICE_ACTION_RESUME);
    LOG_INF("kscan_resume_fix: PM_DEVICE_ACTION_RESUME ret=%d", ret);

    return 0;
}

SYS_INIT(force_kscan_resume, APPLICATION, 95);
