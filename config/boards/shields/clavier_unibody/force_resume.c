/*
 * 標準のzmk,kscan-gpio-matrixドライバは、CONFIG_PM_DEVICEが有効な場合、
 * 「PM_DEVICE_ACTION_RESUME」が発行されて初めて実際のピン設定
 * (kscan_matrix_setup_pins)を行う実装になっている。今晩の調査で、この
 * レジュームが通常の起動シーケンスで自動的には呼ばれていない疑いが
 * 強かった(ピン設定完了を示すログが一度も出なかった)。
 *
 * プルダウン抵抗のハードウェア修正後も標準ファームウェアで無反応だった
 * ことから、この疑いを検証するため、起動時に明示的にRESUMEを発行する。
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(force_resume, LOG_LEVEL_INF);

static int force_kscan_resume(void) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    if (!device_is_ready(kscan_dev)) {
        LOG_ERR("force_kscan_resume: kscan device not ready");
        return 0;
    }

    int ret = pm_device_action_run(kscan_dev, PM_DEVICE_ACTION_RESUME);
    LOG_INF("force_kscan_resume: PM_DEVICE_ACTION_RESUME ret=%d", ret);

    return 0;
}

SYS_INIT(force_kscan_resume, APPLICATION, 95);
