zephyr_library()

# デバッグ用(診断が終わったら削除する)
# zmk/*.h ヘッダを使うため、ZMKのインクルードパスを持つ"app"ターゲットに直接追加する。
target_sources(app PRIVATE pm_monitor.c)
