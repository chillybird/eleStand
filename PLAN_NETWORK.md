# 联网配置实现计划

## 1. 目标

ESP32-C3 SuperMini 接入 WiFi，提供 HTTP 服务替代 USB 串口通信，
实现浏览器直接控制舵机，无需额外网页文件或串口连接。

## 2. 架构

```
┌─────────────────────────────────────────────────────┐
│ ESP32-C3                                             │
│                                                       │
│  WiFi STA ── HTTP Server (port 80)                    │
│    │              │                                    │
│    │         ┌────┴────┐                               │
│    │         │ Static   │  1. GET  / → HTML 页面       │
│    │         │ Files    │  2. POST /api/down           │
│    │         └────┬────┘  3. POST /api/up              │
│    │              │      4. POST /api/stop             │
│    │         ┌────┴────┐  5. POST /api/cw              │
│    │         │ REST API │  6. POST /api/ccw             │
│    │         └────┬────┘  7. POST /api/speed?val=50    │
│    │              │      8. GET  /api/status → JSON     │
│    │         ┌────┴────┐                                │
│    │         │ Command  │ → g_cmd_queue                 │
│    │         │ Dispatch │                                │
│    │         └─────────┘                                │
│                                                       │
│  舵机/开关控制 (servo.c / switch.c / app_main.c)      │
└─────────────────────────────────────────────────────┘
```

## 3. 新增依赖

在 `main/CMakeLists.txt` 的 `PRIV_REQUIRES` 中添加：

```cmake
PRIV_REQUIRES driver esp_timer esp_driver_usb_serial_jtag
             esp_wifi esp_http_server nvs_flash
```

| 组件 | 用途 |
|------|------|
| `esp_wifi` | WiFi STA 模式连接路由器 |
| `esp_http_server` | HTTP 服务器 (轻量, 无依赖 lwIP socket) |
| `nvs_flash` | 存储 WiFi SSID/密码等配置(非易失存储) |

## 4. 新增文件

```
main/
├── wifi.h           # WiFi 连接封装
├── wifi.c           # STA 初始化 + 自动重连
├── http_api.h       # HTTP API 端点 + 静态页面
├── http_api.c       # URI 路由 + 命令投递 + JSON 响应
└── web_page.h       # gzip 压缩的 HTML 页面 (C 数组)
```

## 5. 模块设计

### 5.1 WiFi 模块 (`wifi.h`)

```c
// 连接指定 WiFi (SSID & 密码硬编码或从 NVS 读取)
void wifi_init_sta(const char *ssid, const char *password);

// 等待连接成功 (超时返回 false)
bool wifi_wait_connected(uint32_t timeout_ms);

// 获取当前 IP (如 "192.168.1.100")
const char *wifi_get_ip(void);
```

### 5.2 HTTP API 模块 (`http_api.h`)

```c
void http_server_start(void);

// REST 端点设计:
// GET  /              → text/html  返回控制页面
// POST /api/down      → 投递 CMD_DOWN
// POST /api/up        → 投递 CMD_UP
// POST /api/cw        → 投递 CMD_CW
// POST /api/ccw       → 投递 CMD_CCW
// POST /api/stop      → 投递 CMD_STOP
// POST /api/speed?val=N → 调用 servo_set_speed(N)
// GET  /api/status    → JSON: {"down":1,"up":0}
//
// 所有 POST 返回: {"ok":true}
```

### 5.3 状态 JSON 格式

```json
{
  "down": 1,
  "up": 0,
  "speed": 10
}
```

## 6. 控制页面设计

嵌入在固件中的单页 HTML（内联 CSS/JS，gzip 后 ~2KB），
通过 `GET /` 返回。功能与 `web_serial.html` 一致，
但无需 Web Serial API，直接 fetch POST 到设备 IP。

页面结构：
```
┌──────────────────────┐
│ MG90S 舵机控制        │
│ WiFi: connected       │
├──────────────────────┤
│ 放下 GP3  [绿色/灰色] │
│ 立起 GP5  [绿色/灰色] │
├──────────────────────┤
│ 转速 [====●===]  10% │
├──────────────────────┤
│ [ 放下 ]  [ 立起 ]   │
│ [正转CW] [反转CCW]    │
│ [     停止     ]     │
│ [     调试     ]     │
└──────────────────────┘
```

页面状态每 2 秒通过 `GET /api/status` 轮询更新。

## 7. app_main 改动

```c
void app_main(void) {
    // 原有初始化...
    servo_init(SERVO_GPIO);
    sw_init();
    g_cmd_queue = xQueueCreate(8, sizeof(int));
    xTaskCreate(serial_task, "serial", 4096, NULL, 5, NULL);

    // 新增: WiFi 初始化
    nvs_flash_init();
    wifi_init_sta("SSID", "PASSWORD");      // TODO: 改为可配置
    wifi_wait_connected(30000);

    // 新增: HTTP 服务
    http_server_start();

    // 原有: 初始定位 + 命令循环
    move_to(sw_down_gpio());
    move_poll(sw_down_gpio(), 30000, 20);
    servo_stop();
    // ... 主循环不变 ...
}
```

## 8. 实施步骤

| 步骤 | 内容 | 文件 |
|------|------|------|
| 1 | 创建 `wifi.c/h`, 实现 STA 连接 | wifi.h/c |
| 2 | 创建 `http_api.c/h`, 注册 URI 路由 | http_api.h/c |
| 3 | 创建 `web_page.h`, 嵌入 HTML 页面 | web_page.h |
| 4 | 更新 `CMakeLists.txt` 添加依赖 | CMakeLists.txt |
| 5 | 修改 `app_main.c`, 集成 WiFi + HTTP | app_main.c |
| 6 | 测试: 浏览器访问设备 IP | -- |

## 9. 后续增强（v2）

- WiFi 配置页面 (Captive Portal)
- NVS 持久化 SSID/密码
- WebSocket 替代轮询实现实时推送
- mDNS 域名 (http://eleStand.local)
- OTA 固件升级
- 同时保留 USB 串口控制
