# Snippets

可独立运行的 ESP-IDF 代码片段，每个子目录是一个完整工程。
使用时先 `cp -r 目录名 目标路径`，再 `idf.py set-target esp32c3`。

| 片段 | 说明 | 依赖 |
|------|------|------|
| `_template` | 模板工程，复制后直接改 | 无 |
| `motor_angle_control` | MG90S 180° 位置舵机角度控制 + Web 网页 | driver |
| `wifi_scan` | WiFi 扫描附近热点 | esp_wifi |
| `wifi_ap` | WiFi 热点模式 | esp_wifi |
| `wifi_sta` | WiFi 连接路由器 (修改 SSID/密码后可用) | esp_wifi |
