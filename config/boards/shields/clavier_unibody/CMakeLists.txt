zephyr_library()

# デバッグ用・実験的(診断が終わったら削除する)
# zmk/*.h ヘッダを使うため、ZMKのインクルードパスを持つ"app"ターゲットに
# 直接ソースを追加する(zephyr_library_sourcesではインクルードパスが届かない)。
target_sources(app PRIVATE discharge_scan.c)
