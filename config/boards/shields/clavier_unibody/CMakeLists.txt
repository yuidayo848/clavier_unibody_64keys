zephyr_library()

# zmk/*.h ヘッダを使うため、ZMKのインクルードパスを持つ"app"ターゲットに
# 直接ソースを追加する(zephyr_library_sourcesではインクルードパスが届かない)。
target_sources(app PRIVATE kscan_resume_fix.c)
