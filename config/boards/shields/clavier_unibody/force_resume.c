/* デバッグ用(診断が終わったら削除する): kscanデバイスがPM_DEVICE_STATE_SUSPENDEDの
 * ままピン設定(setup_pins)が一度も走っていない可能性を検証するため、アプリ起動の
 * 最後のタイミングで明示的にPM_DEVICE_ACTION_RESUMEを発行する。 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(force_resume, LOG_LEVEL_DBG);

static int force_kscan_resume(void) {
    const struct device *kscan_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_kscan));

    if (!device_is_ready(kscan_dev)) {
        LOG_ERR("force_kscan_resume: kscan device not ready");
        return 0;
    }

    enum pm_device_state state;
    int state_ret = pm_device_state_get(kscan_dev, &state);
    LOG_INF("force_kscan_resume: current state query ret=%d state=%d", state_ret, state);

    int ret = pm_device_action_run(kscan_dev, PM_DEVICE_ACTION_RESUME);
    LOG_INF("force_kscan_resume: PM_DEVICE_ACTION_RESUME ret=%d", ret);

    return 0;
}

SYS_INIT(force_kscan_resume, APPLICATION, 99);
