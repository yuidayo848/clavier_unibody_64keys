zephyr_library()

# kscanドライバのスキャン無限停止バグ対策(恒久的に必要)。
target_sources(app PRIVATE kscan_watchdog.c)
